# L1 projection (Gurobi)

This folder implements the projection of a nominal distribution `pbar` onto the
intersection of the probability simplex and a single halfspace, using a
weighted L1 objective:

minimize  sum_i w_i * |p_i - pbar_i|
subject to b^T p <= beta, 1^T p = 1, p >= 0.

Algorithm and formulation:
- Introduce epigraph variables t_i >= |p_i - pbar_i|.
- Replace absolute values with linear constraints
  t_i >= p_i - pbar_i and t_i >= -p_i + pbar_i.
- The problem becomes a linear program in (p, t).

Gurobi solves this LP directly.
