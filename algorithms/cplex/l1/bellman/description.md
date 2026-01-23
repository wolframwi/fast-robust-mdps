# L1 Bellman (CPLEX)

This folder implements a robust Bellman update for an L1 ambiguity set using
CPLEX.

Problem form:
- Minimize t subject to t >= z_a^T p_a for each action a.
- For each action, p_a is a probability simplex vector.
- The total L1 budget satisfies sum_i w_i * |p_i - pbar_i| <= kappa.

Algorithm and formulation:
- Introduce epigraph variables y_i >= |p_i - pbar_i|.
- Enforce y_i >= p_i - pbar_i and y_i >= -p_i + pbar_i.
- Constrain the weighted sum of y to be <= kappa.
- Minimize t with linear constraints only, yielding an LP.
