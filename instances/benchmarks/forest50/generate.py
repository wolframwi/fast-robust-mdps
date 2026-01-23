#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from benchmark_generation import _add_transition, _finalize_rewards, _init_arrays, write_instance

import mdptoolbox.example as mdp_examples
import numpy as np


def build_instance():
    P, R = mdp_examples.forest(S=50, r1=4, r2=2, p=0.1)
    n_states = P.shape[1]
    n_actions = P.shape[0]
    reward_sum, probs = _init_arrays(n_states, n_actions)
    for s in range(n_states):
        for a in range(n_actions):
            for sp in range(n_states):
                prob = float(P[a, s, sp])
                if prob > 0.0:
                    _add_transition(reward_sum, probs, s, a, sp, prob, float(R[s, a]))
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
