#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from benchmark_generation import _add_transition, _finalize_rewards, _init_arrays, write_instance

import numpy as np


def build_instance():
    rows, cols = 5, 5
    n_states = rows * cols
    n_actions = 4
    terminal_rewards = {(4, 4): 1.0, (4, 0): -1.0}
    terminal_states = {r * cols + c for r, c in terminal_rewards.keys()}
    moves = {
        0: (0, -1),
        1: (1, 0),
        2: (0, 1),
        3: (-1, 0),
    }
    reward_sum, probs = _init_arrays(n_states, n_actions)
    for s in range(n_states):
        r, c = divmod(s, cols)
        for a in range(n_actions):
            if s in terminal_states:
                _add_transition(reward_sum, probs, s, a, s, 1.0, 0.0)
                continue
            dr, dc = moves[a]
            nr = min(max(r + dr, 0), rows - 1)
            nc = min(max(c + dc, 0), cols - 1)
            sp = nr * cols + nc
            reward = terminal_rewards.get((nr, nc), -0.01)
            _add_transition(reward_sum, probs, s, a, sp, 1.0, reward)
    rewards = _finalize_rewards(reward_sum, probs)
    initial = np.zeros(n_states, dtype=float)
    initial[0] = 1.0
    return {
        "n_states": n_states,
        "n_actions": n_actions,
        "discount": 0.99,
        "rewards": rewards,
        "transitions": probs,
        "initial": initial,
    }


def main() -> int:
    instance = build_instance()
    write_instance(Path(__file__).parent, instance)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
