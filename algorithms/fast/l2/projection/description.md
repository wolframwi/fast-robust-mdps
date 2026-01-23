# Fast L2 projection

This folder implements a fast weighted L2 projection of a nominal distribution
`pbar` onto the simplex-halfspace intersection:

minimize  sum_i w_i * (p_i - pbar_i)^2
subject to b^T p <= beta, 1^T p = 1, p >= 0.

Algorithm overview:
- If pbar^T b <= beta, the constraint is slack and the projection is p* = pbar.
- Otherwise solve the KKT system with an active-set style sweep over sorted b.
  The algorithm maintains sums over the active set and updates them incrementally
  as new indices enter the set.
- For each active-set interval, solve the 2x2 linear system for the dual
  multipliers (alpha, gamma) and check feasibility against the current bounds.
- Recover p via the closed-form thresholded rule
  p_i = max(0, ( -alpha * b_i + gamma + 2 * w_i^2 * pbar_i ) / (2 * w_i^2)).\n+- The implementation treats the provided weights as sigma and uses sigma^2 in\n+  the objective, matching the code path.\n+- The objective is computed as the weighted squared L2 deviation.
