# Fast Burg projection

This folder implements the fast Burg projection operator described in
the paper. The goal is

minimize  sum_i pbar_i * (log pbar_i - log p_i)
subject to b^T p <= beta, 1^T p = 1, p > 0,

with pbar strictly positive.

Algorithm overview:
- If pbar^T b <= beta, the constraint is slack and the projection is
  p* = pbar.
- Otherwise solve the 1D dual problem over alpha in [0, 1) via bisection.
  Define d_i = (b_i - beta) / (beta - min(b)) and
  psi'(alpha) = sum_i pbar_i * d_i / (1 + alpha * d_i).
  psi'(alpha) is strictly decreasing and crosses zero in (0, 1).
- Bisection on psi'(alpha) yields alpha*.
- Recover the primal solution in closed form:
  p_i = pbar_i / (1 + alpha* * d_i), followed by a normalization guard.

The objective is computed as the Burg divergence.
