from __future__ import annotations

from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np


MDPInstance = Dict[str, object]


LEVELS = {
    "mild": 0.05,
    "moderate": 0.10,
    "severe": 0.20,
}


def format_float(value: float) -> str:
    return f"{value:.10g}"


def _init_arrays(n_states: int, n_actions: int) -> Tuple[np.ndarray, np.ndarray]:
    reward_sum = np.zeros((n_states, n_actions, n_states), dtype=float)
    probs = np.zeros((n_states, n_actions, n_states), dtype=float)
    return reward_sum, probs


def _add_transition(
    reward_sum: np.ndarray,
    probs: np.ndarray,
    s: int,
    a: int,
    sp: int,
    prob: float,
    reward: float,
) -> None:
    probs[s, a, sp] += prob
    reward_sum[s, a, sp] += prob * reward


def _finalize_rewards(reward_sum: np.ndarray, probs: np.ndarray) -> np.ndarray:
    rewards = np.zeros_like(reward_sum)
    mask = probs > 0.0
    rewards[mask] = reward_sum[mask] / probs[mask]
    return rewards


def _validate_transitions(transitions: np.ndarray) -> None:
    if np.any(transitions < -1e-12):
        idx = np.unravel_index(np.argmin(transitions), transitions.shape)
        raise ValueError(f"Negative transition probability at {idx}: {transitions[idx]}")
    row_sums = transitions.sum(axis=2)
    max_error = np.max(np.abs(row_sums - 1.0))
    if max_error > 1e-8:
        s, a = np.unravel_index(np.argmax(np.abs(row_sums - 1.0)), row_sums.shape)
        raise ValueError(
            f"Transition row does not sum to 1 at (s={s}, a={a}): {row_sums[s, a]}"
        )


def _validate_initial(initial: np.ndarray, n_states: int) -> None:
    if initial.shape[0] != n_states:
        raise ValueError(
            f"Initial distribution length {initial.shape[0]} does not match n_states {n_states}"
        )
    if np.any(initial < -1e-12):
        idx = int(np.argmin(initial))
        raise ValueError(f"Negative initial probability at {idx}: {initial[idx]}")
    total = float(np.sum(initial))
    if abs(total - 1.0) > 1e-8:
        raise ValueError(f"Initial distribution does not sum to 1: {total}")


def write_mdp(path: Path, instance: MDPInstance) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    transitions = instance["transitions"]
    rewards = instance["rewards"]
    n_states = int(instance["n_states"])
    n_actions = int(instance["n_actions"])
    discount = float(instance["discount"])
    initial = instance["initial"]
    _validate_transitions(transitions)
    _validate_initial(initial, n_states)

    with path.open("w") as fd:
        fd.write(
            f"{n_states} {n_actions} {format_float(discount)}\n"
        )
        fd.write("INITIAL\n")
        fd.write(" ".join(format_float(v) for v in initial))
        fd.write("\n")
        fd.write("REWARDS\n")
        for s in range(n_states):
            for a in range(n_actions):
                fd.write(
                    " ".join(format_float(r) for r in rewards[s, a])
                )
                fd.write("\n")
        fd.write("TRANSITIONS\n")
        for s in range(n_states):
            for a in range(n_actions):
                fd.write(
                    " ".join(format_float(p) for p in transitions[s, a])
                )
                fd.write("\n")


def write_ambiguity(path: Path, instance: MDPInstance, kappa_l1: float) -> None:
    kappa_l2 = kappa_l1 * kappa_l1
    kappa_kl = 0.5 * kappa_l2
    kappa_burg = 0.5 * kappa_l2
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as fd:
        fd.write(f"L1 {format_float(kappa_l1)}\n")
        fd.write(f"L2 {format_float(kappa_l2)}\n")
        fd.write(f"KL {format_float(kappa_kl)}\n")
        fd.write(f"BURG {format_float(kappa_burg)}\n")
        fd.write("SIGMA_L1\n")
        n_states = int(instance["n_states"])
        n_actions = int(instance["n_actions"])
        for _s in range(n_states):
            for _a in range(n_actions):
                fd.write(" ".join("1" for _ in range(n_states)))
                fd.write("\n")
        fd.write("SIGMA_L2\n")
        for _s in range(n_states):
            for _a in range(n_actions):
                fd.write(" ".join("1" for _ in range(n_states)))
                fd.write("\n")


def write_ambiguity_set(output_dir: Path, instance: MDPInstance) -> None:
    for level, kappa in LEVELS.items():
        write_ambiguity(output_dir / f"{level}.amb", instance, kappa)


def write_instance(output_dir: Path, instance: MDPInstance) -> None:
    write_mdp(output_dir / "nominal.mdp", instance)
    write_ambiguity_set(output_dir, instance)
