# KL projection (MOSEK)

This folder implements the KL projection onto a simplex-halfspace
intersection:

minimize  sum_i p_i * log(p_i / pbar_i)
subject to b^T p <= beta, 1^T p = 1, p >= 0,

with pbar strictly positive.

Algorithm and formulation:
- Introduce epigraph variables t_i for the relative-entropy terms.
- Each constraint t_i >= p_i * log(p_i / pbar_i) is represented using
  MOSEK's relative-entropy (exponential-cone) modeling.
- Minimize sum_i t_i subject to the linear constraints on p.

This is the exact conic formulation described in
`/home/wwiesema/KL_projection.md`.
