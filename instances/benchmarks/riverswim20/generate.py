#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from benchmark_generation import _add_transition, _finalize_rewards, _init_arrays, write_instance

import numpy as np


def build_instance():
    n_states = 20
    n_actions = 2
    reward_sum, probs = _init_arrays(n_states, n_actions)
    for s in range(n_states):
        for a in range(n_actions):
            if a == 0:
                if s == 0:
                    next_states = [(0, 1.0)]
                else:
                    next_states = [(s - 1, 1.0)]
            else:
                if s == 0:
                    next_states = [(0, 0.6), (1, 0.4)]
                elif s == n_states - 1:
                    next_states = [(n_states - 1, 0.6), (n_states - 2, 0.4)]
                else:
                    next_states = [(s + 1, 0.05), (s, 0.6), (s - 1, 0.35)]
            reward = 0.0
            if s == 0 and a == 0:
                reward = 0.005
            if s == n_states - 1 and a == 1:
                reward = 1.0
            for sp, prob in next_states:
                _add_transition(reward_sum, probs, s, a, sp, prob, reward)
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
