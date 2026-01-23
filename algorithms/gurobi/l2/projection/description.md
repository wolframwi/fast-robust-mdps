# L2 projection (Gurobi)

This folder implements the projection of a nominal distribution `pbar` onto the
intersection of the probability simplex and a single halfspace, using a
weighted L2 objective. The solver uses the squared L2 form:

minimize  sum_i w_i * (p_i - pbar_i)^2
subject to b^T p <= beta, 1^T p = 1, p >= 0.

Algorithm and formulation:
- The squared L2 objective is a convex quadratic function.
- The constraints are linear.
- This yields a convex QP that Gurobi solves exactly.

The minimizer of the squared L2 objective is the same as the minimizer of the
unsquared L2 norm, so this QP gives the correct projection point.
