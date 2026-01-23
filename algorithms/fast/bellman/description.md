# Fast robust Bellman (projection oracle)

This folder implements a robust Bellman update for a single state using a
projection oracle for the ambiguity set. The update solves

minimize  t
subject to t >= sum_{s'} p_{a}(s') * z_{a}(s')  for each action a,
         sum_{a} D(p_a || pbar_a) <= kappa,
         p_a in simplex,

where z_a(s') = r(s,a,s') + gamma * v(s') and D is the divergence handled by
the projection callback.

Algorithm overview:
- Precompute z_a(s') for each action and derive lower/upper bounds from the
  min/max of z_a.
- Bisection on the scalar threshold theta in [lower, upper].
- For each action, project pbar_a onto {p: z_a^T p <= theta} using the provided
  projection callback; accumulate the projection objective values.
- If the total divergence sum <= kappa, the theta is feasible and the upper
  bound is lowered; otherwise increase the lower bound.

The routine returns the smallest theta within tolerance.
