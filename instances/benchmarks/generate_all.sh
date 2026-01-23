#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

python "${ROOT}/blackjack/generate.py"
python "${ROOT}/capacity50/generate.py"
python "${ROOT}/chain10/generate.py"
python "${ROOT}/cliffwalking/generate.py"
python "${ROOT}/forest50/generate.py"
python "${ROOT}/frozenlake4x4/generate.py"
python "${ROOT}/frozenlake8x8/generate.py"
python "${ROOT}/gridworld25/generate.py"
python "${ROOT}/inventory50/generate.py"
python "${ROOT}/machine20/generate.py"
python "${ROOT}/openspiel_grid16/generate.py"
python "${ROOT}/perishable50/generate.py"
python "${ROOT}/riverswim6/generate.py"
python "${ROOT}/riverswim20/generate.py"
python "${ROOT}/taxi/generate.py"
