# Burg projection (Gurobi)

This folder implements the Burg projection onto a simplex-halfspace
intersection:

minimize  sum_i pbar_i * (log pbar_i - log p_i)
subject to b^T p <= beta, 1^T p = 1, p > 0,

with pbar strictly positive.

Algorithm and formulation:
- Introduce variables y_i = log(p_i) using Gurobi's general log constraint.
- Minimize the linear objective -sum_i pbar_i * y_i.
- This is a nonlinear (log) model; the implementation sets NonConvex=2 to let
  Gurobi handle it.
- After solving, the code adds the constant sum_i pbar_i * log(pbar_i) to
  recover the full Burg divergence value.

Gurobi solves this formulation directly and returns the projection and
objective.
