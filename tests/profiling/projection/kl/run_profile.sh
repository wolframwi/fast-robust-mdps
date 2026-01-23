#!/usr/bin/env bash
set -euo pipefail

make
./kl_profile

# Example perf usage:
# perf record -g -- ./kl_profile
# perf report
