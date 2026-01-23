#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: run_vi_l1_synthetic_homotopy.sh

Runs L1 homotopy value iteration over synthetic instances with A=10 and A=S
across all ambiguity sets.
EOF
}

if [ "${#}" -ne 0 ]; then
  usage
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
base_dir="$(cd "${script_dir}/.." && pwd)"
root="${ROOT:-${base_dir}/../../../..}"
instances_root="${root}/instances/synthetic"
runner="${script_dir}/run_vi_l1_homotopy_one.sh"

if [ ! -d "${instances_root}" ]; then
  echo "Synthetic instances not found at ${instances_root}" >&2
  exit 1
fi

if [ ! -x "${runner}" ]; then
  echo "Runner not executable: ${runner}" >&2
  exit 1
fi

while IFS= read -r -d '' mdp_path; do
  inst_dir="$(dirname "${mdp_path}")"
  if [[ "${inst_dir}" =~ /S=([0-9]+)/A=([0-9]+) ]]; then
    s="${BASH_REMATCH[1]}"
    a="${BASH_REMATCH[2]}"
    if [ "${a}" -eq 10 ] || [ "${a}" -eq "${s}" ]; then
      for amb in "${inst_dir}"/*.amb; do
        [ -f "${amb}" ] || continue
        "${runner}" "${inst_dir}" "${amb}"
      done
    fi
  fi
done < <(find "${instances_root}" -type f -name nominal.mdp -print0)
