#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: run_vi_l2_one.sh <instance_dir> <amb_file> <method>

Runs one L2 value-iteration method and merges the results into runtime/errors.
Methods: fast | cplex | gurobi | mosek | all
EOF
}

if [ "${#}" -ne 3 ]; then
  usage
  exit 1
fi

instance_dir="$1"
amb_file="$2"
method="$3"

case "${method}" in
  fast|cplex|gurobi|mosek|all) ;;
  *) echo "Unknown method: ${method}" >&2; usage; exit 1 ;;
esac

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
base_dir="$(cd "${script_dir}/.." && pwd)"
runtime_dir="${base_dir}/runtime"
error_dir="${runtime_dir}/errors"
mkdir -p "${runtime_dir}" "${error_dir}"

sanitize_name() {
  local path="$1"
  local out=""
  local i c
  for ((i = 0; i < ${#path}; i++)); do
    c="${path:i:1}"
    case "${c}" in
      '/'|'\\'|':'|' ') out+="_" ;;
      '.') ;;
      *) out+="${c}" ;;
    esac
  done
  printf '%s' "${out}"
}

base_name="$(sanitize_name "${instance_dir}")"
amb_base="${amb_file##*/}"
amb_stem="${amb_base%.amb}"
runtime_path="${runtime_dir}/${base_name}__${amb_stem}.txt"
error_path="${error_dir}/${base_name}__${amb_stem}.log"

binary="${runtime_dir}/vi_l2_single"
source="${base_dir}/run_vi_l2_single.cpp"

if [ ! -x "${binary}" ] || [ "${source}" -nt "${binary}" ]; then
  cxx="${CXX:-g++}"
  root="${ROOT:-${base_dir}/../../../..}"
  cxxflags="${CXXFLAGS:--std=c++17 -O3 -DNDEBUG -march=native -g -I${root} -DIL_STD -DUSE_CPLEX}"

  cplex_root="${CPLEX_ROOT:-/home/wwiesema/CPLEX_Studio2211}"
  mosek_root="${MOSEK_ROOT:-/home/wwiesema/mosek/11.1/tools/platform/linux64x86}"
  gurobi_home="${GUROBI_HOME:-}"
  if [ -z "${gurobi_home}" ]; then
    shopt -s nullglob
    candidates=(/home/wwiesema/gurobi*/linux64 /opt/gurobi*/linux64)
    shopt -u nullglob
    if [ "${#candidates[@]}" -gt 0 ]; then
      gurobi_home="$(printf '%s\n' "${candidates[@]}" | sort -V | tail -n 1)"
    fi
  fi

  cplex_inc="${cplex_root}/cplex/include"
  concert_inc="${cplex_root}/concert/include"
  cplex_lib="${cplex_root}/cplex/lib/x86-64_linux/static_pic"
  concert_lib="${cplex_root}/concert/lib/x86-64_linux/static_pic"
  mosek_inc="${mosek_root}/h"
  mosek_lib="${mosek_root}/bin"
  gurobi_inc="${gurobi_home}/include"
  gurobi_lib="${gurobi_home}/lib"

  sources=(
    "${source}"
    "${root}/algorithms/mdp.cpp"
    "${root}/algorithms/fast/bellman/bellman.cpp"
    "${root}/algorithms/fast/l2/projection/project.cpp"
    "${root}/algorithms/cplex/l2/bellman/bellman.cpp"
    "${root}/algorithms/gurobi/l2/bellman/bellman.cpp"
    "${root}/algorithms/mosek/l2/bellman/bellman.cpp"
  )

  ldflags=(
    "-L${cplex_lib}" "-L${concert_lib}"
    "-L${mosek_lib}"
    "-L${gurobi_lib}"
    "-Wl,-rpath,${mosek_lib}"
    "-Wl,-rpath,${gurobi_lib}"
  )
  ldlibs=(
    -lilocplex -lcplex -lconcert
    -lfusion64 -lmosek64
    -lgurobi_c++ -lgurobi130 -lpthread
  )

  "${cxx}" ${cxxflags} -I"${cplex_inc}" -I"${concert_inc}" -I"${mosek_inc}" \
    -I"${gurobi_inc}" "${sources[@]}" "${ldflags[@]}" "${ldlibs[@]}" -o "${binary}"
fi

tmp_runtime="$(mktemp "${runtime_dir}/.tmp_${base_name}__${amb_stem}_${method}.XXXXXX")"
tmp_error="$(mktemp "${error_dir}/.tmp_${base_name}__${amb_stem}_${method}.XXXXXX")"

(cd "${base_dir}" && "${binary}" \
  --instance "${instance_dir}" \
  --amb "${amb_file}" \
  --methods "${method}" \
  --runtime-out "${tmp_runtime}" \
  --error-out "${tmp_error}")

if [ -f "${runtime_path}" ]; then
  python3 - "${runtime_path}" "${tmp_runtime}" <<'PY'
import os
import sys

def load(path):
  data = {}
  order = []
  if not os.path.exists(path):
    return data, order
  with open(path, "r", encoding="utf-8") as f:
    for line in f:
      line = line.strip()
      if not line or "=" not in line:
        continue
      key, value = line.split("=", 1)
      if key not in order:
        order.append(key)
      data[key] = value
  return data, order

dest_path, src_path = sys.argv[1], sys.argv[2]
dest, dest_order = load(dest_path)
src, src_order = load(src_path)

dest.update(src)
for key in src_order:
  if key not in dest_order:
    dest_order.append(key)

with open(dest_path, "w", encoding="utf-8") as f:
  for key in dest_order:
    if key in dest:
      f.write(f"{key}={dest[key]}\n")
PY
else
  mv "${tmp_runtime}" "${runtime_path}"
fi

if [ -f "${error_path}" ]; then
  awk '($0 !~ /^(instance|amb)=/ && $0 != "") {print}' "${tmp_error}" >> "${error_path}"
else
  mv "${tmp_error}" "${error_path}"
fi

rm -f "${tmp_runtime}" "${tmp_error}"
