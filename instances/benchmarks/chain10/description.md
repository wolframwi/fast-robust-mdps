# chain10

Source: Standard chain / random-walk MDP used as a toy benchmark in DP and
robust MDP literature.

10 states in a line. Two actions bias motion left/right: left action moves left
with prob 0.9 and right with 0.1; right action reverses these. Small reward 0.2
for taking left action in state 0, and reward 1.0 for taking right action in the
last state. All other rewards are 0.

Initial distribution: deterministic at state 0.
