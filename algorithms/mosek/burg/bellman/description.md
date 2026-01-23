# Burg Bellman (MOSEK)

This folder implements a robust Bellman update for a Burg ambiguity set using
MOSEK Fusion.

Problem form:
- Minimize t subject to t >= z_a^T p_a for each action a.
- For each action, p_a is a probability simplex vector with pbar_a > 0.
- The Burg budget satisfies sum_i pbar_i * (log pbar_i - log p_i) <= kappa.

Algorithm and formulation:
- Introduce variables s_i >= -log(p_i).
- Enforce (p_i, 1, -s_i) in the primal exponential cone, which encodes
  s_i >= -log(p_i).
- The Burg constraint becomes dot(pbar, s) <= sum_i pbar_i log(pbar_i) - kappa.
- Minimize t with linear constraints and exponential-cone constraints.

This is the exact conic formulation solved by MOSEK.
