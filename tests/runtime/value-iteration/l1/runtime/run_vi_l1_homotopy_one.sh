#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: run_vi_l1_homotopy_one.sh <instance_dir> <amb_file>

Runs one L1 homotopy value-iteration instance and merges results into runtime_homotopy/errors_homotopy.
EOF
}

if [ "${#}" -ne 2 ]; then
  usage
  exit 1
fi

instance_dir="$1"
amb_file="$2"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
base_dir="$(cd "${script_dir}/.." && pwd)"
runtime_dir="${base_dir}/runtime_homotopy"
error_dir="${base_dir}/errors_homotopy"
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

binary="${runtime_dir}/vi_l1_homotopy_single"
source="${base_dir}/run_vi_l1_homotopy_single.cpp"

if [ ! -x "${binary}" ] || [ "${source}" -nt "${binary}" ]; then
  cxx="${CXX:-g++}"
  root="${ROOT:-${base_dir}/../../../..}"
  cxxflags="${CXXFLAGS:--std=c++17 -O3 -DNDEBUG -march=native -g -I${root}}"

  sources=(
    "${source}"
    "${root}/algorithms/mdp.cpp"
    "${root}/algorithms/homotopy/bellman/bellman.cpp"
  )

  "${cxx}" ${cxxflags} "${sources[@]}" -o "${binary}"
fi

tmp_runtime="$(mktemp "${runtime_dir}/.tmp_${base_name}__${amb_stem}.XXXXXX")"
tmp_error="$(mktemp "${error_dir}/.tmp_${base_name}__${amb_stem}.XXXXXX")"

(cd "${base_dir}" && "${binary}" \
  --instance "${instance_dir}" \
  --amb "${amb_file}" \
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
