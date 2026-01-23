# Burg projection (MOSEK)

This folder implements the Burg projection onto a simplex-halfspace
intersection:

minimize  sum_i pbar_i * (log pbar_i - log p_i)
subject to b^T p <= beta, 1^T p = 1, p > 0,

with pbar strictly positive.

Algorithm and formulation:
- Introduce epigraph variables s_i >= -log(p_i).
- Enforce s_i >= -log(p_i) with exponential-cone constraints
  (-s_i, 1, p_i) in K_exp.
- The objective becomes minimize sum_i pbar_i * s_i with linear constraints
  on p.

This is the exact conic formulation described in
`/home/wwiesema/Burg_projection.md`.
