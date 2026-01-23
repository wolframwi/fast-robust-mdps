# L1 homotopy Bellman

This folder implements a robust Bellman update for an L1 ambiguity set using a
homotopy (piecewise-linear) construction per action.

Problem form:
- For each action a, the worst-case expectation over transitions p_a is
  computed under the constraint sum_i w_i * |p_i - pbar_i| <= xi.
- The global Bellman update chooses the smallest t such that the total budget
  sum_a xi_a <= kappa.

Algorithm overview:
- For each action, build a piecewise-linear curve that maps L1 radius xi to the
  worst-case value q(xi) by transferring probability mass between states.
- Invert each curve to compute xi_a(t) (the smallest radius that achieves
  value <= t).
- Use bisection on t to satisfy sum_a xi_a(t) <= kappa.

The implementation tracks the curve segments explicitly and uses the inverse
mapping during the bisection.
