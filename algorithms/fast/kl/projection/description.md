# Fast KL projection

This folder implements the fast KL projection operator described in
the paper. The goal is

minimize  KL(p || pbar)
subject to b^T p <= beta, 1^T p = 1, p >= 0,

with pbar strictly positive.

Algorithm overview:
- If pbar^T b <= beta, the constraint is slack and the projection is
  p* = pbar.
- Otherwise solve the 1D dual problem in alpha >= 0 via bisection.
  The monotone equation is E_{p(alpha)}[b] = beta, where
  p(alpha)_i = pbar_i * exp(-alpha * b_i) / Z(alpha).
- The implementation brackets alpha using the paper's upper bound
  alpha_U = log(1 / min(pbar)) / (beta - min(b)).
- Each bisection step computes p(alpha) with a log-sum-exp trick for
  numerical stability, then updates the bracket using the sign of
  E_{p(alpha)}[b] - beta.

The final p(alpha_U) is returned, and the KL objective is computed as
sum_i p_i * (log p_i - log pbar_i).
