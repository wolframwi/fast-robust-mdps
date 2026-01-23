# Value iteration

This header implements a generic value-iteration loop for a provided Bellman
operator.

Algorithm overview:
- Initialize v = 0 and repeatedly apply v_next[s] = bellman(s, v).
- Track the maximum state-wise change and stop when it is <= tolerance.
- Optionally log progress every N iterations and enforce a maximum iteration
  cap (throwing if the cap is reached before convergence).

The result includes the converged value vector and iteration count.
