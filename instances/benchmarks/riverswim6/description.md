# riverswim6

Source: RiverSwim benchmark (Strehl & Littman, 2008).

6 states in a line. Left action moves left deterministically with a boundary
self-loop at state 0; right action is stochastic (0.05 right, 0.6 stay, 0.35
left) except at the ends. Reward 0.005 for taking left in state 0, reward 1.0
for taking right in the last state.

Initial distribution: deterministic at state 0.
