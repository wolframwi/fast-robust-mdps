# frozenlake8x8

Source: Gymnasium `FrozenLake-v1` (toy_text), 8x8 map.

Imported directly from Gymnasium by reading `env.unwrapped.P` with
`is_slippery=True`. Rewards are 1 on goal, 0 otherwise. Actions are the four
cardinal moves.

Initial distribution: Gymnasium `env.unwrapped.isd` (deterministic start state).
