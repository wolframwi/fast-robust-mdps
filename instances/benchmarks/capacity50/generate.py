#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from benchmark_generation import _add_transition, _finalize_rewards, _init_arrays, write_instance

import math
import numpy as np


def build_instance():
    capacity = 49
    prices = [1.0, 2.0, 3.0, 4.0, 5.0]
    lambdas = [4.0, 3.0, 2.5, 2.0, 1.5]
    n_states = capacity + 1
    n_actions = len(prices)
    max_demand = 5
    demands = list(range(max_demand + 1))
    demand_probs_by_action = []
    for lam in lambdas:
        probs = []
        for d in range(max_demand):
            probs.append(math.exp(-lam) * (lam ** d) / math.factorial(d))
        tail = max(0.0, 1.0 - sum(probs))
        probs.append(tail)
        demand_probs_by_action.append(probs)

    reward_sum, probs = _init_arrays(n_states, n_actions)
    for s in range(n_states):
        for a in range(n_actions):
            if s == 0:
                _add_transition(reward_sum, probs, s, a, 0, 1.0, 0.0)
                continue
            price = prices[a]
            demand_probs = demand_probs_by_action[a]
            for demand, prob in zip(demands, demand_probs):
                sold = min(s, demand)
                sp = s - sold
                reward = price * sold
                _add_transition(reward_sum, probs, s, a, sp, prob, reward)
    rewards = _finalize_rewards(reward_sum, probs)
    initial = np.zeros(n_states, dtype=float)
    initial[-1] = 1.0
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
