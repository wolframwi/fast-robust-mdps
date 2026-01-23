#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from benchmark_generation import _add_transition, _finalize_rewards, _init_arrays, write_instance

import numpy as np


def _add_card(total: int, usable: bool, card: int):
    if card == 1:
        if total + 11 <= 21:
            total += 11
            usable = True
        else:
            total += 1
    else:
        total += card
    if total > 21 and usable:
        total -= 10
        usable = False
    return total, usable


def _dealer_outcomes(show_card: int):
    card_probs = {
        1: 1.0 / 13.0,
        2: 1.0 / 13.0,
        3: 1.0 / 13.0,
        4: 1.0 / 13.0,
        5: 1.0 / 13.0,
        6: 1.0 / 13.0,
        7: 1.0 / 13.0,
        8: 1.0 / 13.0,
        9: 1.0 / 13.0,
        10: 4.0 / 13.0,
    }

    def dealer_policy(total: int, usable: bool):
        if total >= 17:
            return {-1: 1.0} if total > 21 else {total: 1.0}
        dist = {}
        for card, prob in card_probs.items():
            new_total, new_usable = _add_card(total, usable, card)
            for outcome, p_out in dealer_policy(new_total, new_usable).items():
                dist[outcome] = dist.get(outcome, 0.0) + prob * p_out
        return dist

    show_total, show_usable = _add_card(0, False, show_card)
    dist = {}
    for card, prob in card_probs.items():
        total, usable = _add_card(show_total, show_usable, card)
        for outcome, p_out in dealer_policy(total, usable).items():
            dist[outcome] = dist.get(outcome, 0.0) + prob * p_out
    return dist


def _initial_distribution():
    card_probs = {
        1: 1.0 / 13.0,
        2: 1.0 / 13.0,
        3: 1.0 / 13.0,
        4: 1.0 / 13.0,
        5: 1.0 / 13.0,
        6: 1.0 / 13.0,
        7: 1.0 / 13.0,
        8: 1.0 / 13.0,
        9: 1.0 / 13.0,
        10: 4.0 / 13.0,
    }
    player_sums = list(range(4, 22))
    dealer_shows = list(range(1, 11))
    usable_aces = [0, 1]
    n_states = len(player_sums) * len(dealer_shows) * len(usable_aces) + 3

    def encode(player_sum: int, dealer_show: int, usable: int) -> int:
        return ((player_sum - 4) * 10 + (dealer_show - 1)) * 2 + usable

    initial = np.zeros(n_states, dtype=float)
    for p1, p1_prob in card_probs.items():
        for p2, p2_prob in card_probs.items():
            prob = p1_prob * p2_prob
            total, usable = _add_card(0, False, p1)
            total, usable = _add_card(total, usable, p2)
            state_idx = encode(total, 1, 0)
            for d, d_prob in card_probs.items():
                state_idx = encode(total, d, int(usable))
                initial[state_idx] += prob * d_prob
    return initial


def build_instance():
    card_probs = {
        1: 1.0 / 13.0,
        2: 1.0 / 13.0,
        3: 1.0 / 13.0,
        4: 1.0 / 13.0,
        5: 1.0 / 13.0,
        6: 1.0 / 13.0,
        7: 1.0 / 13.0,
        8: 1.0 / 13.0,
        9: 1.0 / 13.0,
        10: 4.0 / 13.0,
    }
    player_sums = list(range(4, 22))
    dealer_shows = list(range(1, 11))
    usable_aces = [0, 1]
    base_states = len(player_sums) * len(dealer_shows) * len(usable_aces)
    win_state = base_states
    lose_state = base_states + 1
    draw_state = base_states + 2

    def encode(player_sum: int, dealer_show: int, usable: int) -> int:
        return ((player_sum - 4) * 10 + (dealer_show - 1)) * 2 + usable

    n_states = base_states + 3
    n_actions = 2
    reward_sum, probs = _init_arrays(n_states, n_actions)

    dealer_cache = {show: _dealer_outcomes(show) for show in dealer_shows}

    for ps in player_sums:
        for ds in dealer_shows:
            for ua in usable_aces:
                s = encode(ps, ds, ua)
                outcomes = dealer_cache[ds]
                for dealer_total, prob in outcomes.items():
                    if dealer_total == -1 or dealer_total < ps:
                        _add_transition(reward_sum, probs, s, 0, win_state, prob, 1.0)
                    elif dealer_total > ps:
                        _add_transition(reward_sum, probs, s, 0, lose_state, prob, -1.0)
                    else:
                        _add_transition(reward_sum, probs, s, 0, draw_state, prob, 0.0)

                for card, prob in card_probs.items():
                    new_sum, new_usable = _add_card(ps, bool(ua), card)
                    if new_sum > 21:
                        _add_transition(reward_sum, probs, s, 1, lose_state, prob, -1.0)
                    else:
                        sp = encode(new_sum, ds, int(new_usable))
                        _add_transition(reward_sum, probs, s, 1, sp, prob, 0.0)

    for terminal in (win_state, lose_state, draw_state):
        for a in range(n_actions):
            _add_transition(reward_sum, probs, terminal, a, terminal, 1.0, 0.0)

    rewards = _finalize_rewards(reward_sum, probs)
    initial = _initial_distribution()
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
