#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from benchmark_generation import _add_transition, _finalize_rewards, _init_arrays, write_instance

import numpy as np


def build_instance():
    rows, cols = 4, 4
    n_states = rows * cols
    n_actions = 5
    goal = (rows - 1, cols - 1)
    step_reward = -0.01
    solve_reward = 100.0
    group_reward = 100.0
    goal_reward = solve_reward + group_reward
    moves = {
        0: (0, 0),
        1: (0, -1),
        2: (-1, 0),
        3: (0, 1),
        4: (1, 0),
    }
    reward_sum, probs = _init_arrays(n_states, n_actions)
    for s in range(n_states):
        r, c = divmod(s, cols)
        for a in range(n_actions):
            dr, dc = moves[a]
            nr = min(max(r + dr, 0), rows - 1)
            nc = min(max(c + dc, 0), cols - 1)
            sp = nr * cols + nc
            if (r, c) == goal:
                _add_transition(reward_sum, probs, s, a, s, 1.0, 0.0)
            elif (nr, nc) == goal:
                _add_transition(reward_sum, probs, s, a, sp, 1.0, goal_reward)
            else:
                _add_transition(reward_sum, probs, s, a, sp, 1.0, step_reward)
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
