#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from benchmark_generation import _add_transition, _finalize_rewards, _init_arrays, write_instance

import gymnasium as gym
import numpy as np


def _gym_initial(env: gym.Env) -> np.ndarray:
    if hasattr(env.unwrapped, "isd"):
        return np.array(env.unwrapped.isd, dtype=float)
    if hasattr(env.unwrapped, "initial_state_distrib"):
        return np.array(env.unwrapped.initial_state_distrib, dtype=float)
    raise AttributeError("Gym environment has no initial state distribution")


def _gym_instance(env: gym.Env, discount: float):
    env.reset()
    n_states = env.observation_space.n
    n_actions = env.action_space.n
    reward_sum, probs = _init_arrays(n_states, n_actions)
    P = env.unwrapped.P
    for s in range(n_states):
        for a in range(n_actions):
            for prob, sp, reward, _terminated in P[s][a]:
                if prob > 0.0:
                    _add_transition(reward_sum, probs, s, a, sp, float(prob), float(reward))
    rewards = _finalize_rewards(reward_sum, probs)
    initial = _gym_initial(env)
    return {
        "n_states": n_states,
        "n_actions": n_actions,
        "discount": discount,
        "rewards": rewards,
        "transitions": probs,
        "initial": initial,
    }


def build_instance():
    env = gym.make("FrozenLake-v1", map_name="4x4", is_slippery=True)
    try:
        return _gym_instance(env, discount=0.99)
    finally:
        env.close()


def main() -> int:
    instance = build_instance()
    write_instance(Path(__file__).parent, instance)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
