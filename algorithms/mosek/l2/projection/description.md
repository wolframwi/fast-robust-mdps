# L2 projection (MOSEK)

This folder implements the projection of a nominal distribution `pbar` onto the
intersection of the probability simplex and a single halfspace, using a
weighted L2 objective. The solver uses the squared L2 form:

minimize  sum_i w_i * (p_i - pbar_i)^2
subject to b^T p <= beta, 1^T p = 1, p >= 0.

Algorithm and formulation:
- The squared L2 objective is convex quadratic; constraints are linear.
- This yields a convex QP that MOSEK solves exactly.
- The minimizer matches the unsquared L2 projection because x -> x^2 is
  monotone on [0, infinity).

See `/home/wwiesema/L2_projection.md` for the modeling notes.
