# L2 Bellman (CPLEX)

This folder implements a robust Bellman update for a weighted L2 ambiguity set
using CPLEX.

Problem form:
- Minimize t subject to t >= z_a^T p_a for each action a.
- For each action, p_a is a probability simplex vector.
- The total L2 budget satisfies sum_a ||W_a (p_a - pbar_a)||_2^2 <= kappa.

Algorithm and formulation:
- For each action, introduce an auxiliary eta_a bounding the weighted L2 norm.
- Add a quadratic constraint encoding
  ||W_a (p_a - pbar_a)||_2^2 <= eta_a.
- Constrain sum_a eta_a <= kappa and minimize t.

This yields a convex QP/QCQP that CPLEX solves directly.
