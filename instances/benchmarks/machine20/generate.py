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
        keep_reward = -(0.1 + 0.02 * s)
        replace_reward = -2.0
        next_keep = min(s + 1, n_states - 1)
        _add_transition(reward_sum, probs, s, 0, next_keep, 1.0, keep_reward)
        _add_transition(reward_sum, probs, s, 1, 0, 1.0, replace_reward)
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
