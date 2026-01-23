#!/usr/bin/env bash
set -euo pipefail

make
./burg_profile

# Example perf usage:
# perf record -g -- ./burg_profile
# perf report
