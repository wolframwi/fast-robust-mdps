# KL projection (Gurobi)

This folder implements the KL projection onto a simplex-halfspace intersection:

minimize  sum_i p_i * log(p_i / pbar_i)
subject to b^T p <= beta, 1^T p = 1, p >= 0,

with pbar strictly positive.

Algorithm and formulation:
- Introduce variables y_i = log(p_i) using Gurobi's general log constraint.
- Enforce t_i = p_i * y_i via bilinear (quadratic) constraints and minimize
  sum_i t_i - sum_i p_i * log(pbar_i).
- This yields a nonconvex quadratic model; the implementation sets
  NonConvex=2 to let Gurobi handle it.

Gurobi solves this formulation directly and returns the projection and
objective.
