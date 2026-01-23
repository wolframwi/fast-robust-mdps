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
    n_states = 50
    max_order = 4
    demand_probs = [0.1, 0.2, 0.3, 0.2, 0.15, 0.05]
    spoilage_rate = 0.2
    n_actions = max_order + 1
    max_inventory = n_states - 1
    demands = list(range(len(demand_probs)))
    order_cost = 0.6
    holding_cost = 0.1
    shortage_cost = 1.2
    reward_sum, probs = _init_arrays(n_states, n_actions)
    for s in range(n_states):
        for a in range(n_actions):
            order = a
            on_hand = min(max_inventory, s + order)
            usable = int(math.floor(on_hand * (1.0 - spoilage_rate)))
            for demand, prob in zip(demands, demand_probs):
                sold = min(usable, demand)
                sp = usable - sold
                shortage = max(demand - usable, 0)
                reward = -(
                    order_cost * order + holding_cost * sp + shortage_cost * shortage
                )
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
