# gridworld25

Source: Standard textbook gridworld benchmark.

5x5 deterministic grid. Actions are the four cardinal moves with wall bounce.
Terminal rewards: +1 at (4,4) and -1 at (4,0), received upon entering. Terminal
states are absorbing with a zero reward thereafter. Step reward -0.01 elsewhere.

Initial distribution: deterministic at the top-left cell (0,0).
