# KL Bellman (MOSEK)

This folder implements a robust Bellman update for a KL ambiguity set using
MOSEK Fusion.

Problem form:
- Minimize t subject to t >= z_a^T p_a for each action a.
- For each action, p_a is a probability simplex vector with pbar_a > 0.
- The total KL budget satisfies sum_i KL(p_i || pbar_i) <= kappa.

Algorithm and formulation:
- Introduce epigraph variables u_i for the relative-entropy terms.
- Enforce (pbar_i, p_i, -u_i) in the primal exponential cone, which encodes
  u_i >= p_i * log(p_i / pbar_i).
- Constrain sum_i u_i <= kappa and minimize t.

This is the exact conic formulation solved by MOSEK.
