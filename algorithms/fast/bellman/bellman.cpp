#include "bellman.hpp"

#include <algorithm>
#include <stdexcept>

namespace fast_bellman {

namespace {

int idx(int a, int sp, int n_states) {
  return a * n_states + sp;
}

} // namespace

double solve_robust_bellman_state(const BellmanStateInput& input,
                                  const ProjectionFn& projection) {
  const int S = input.n_states;
  const int A = input.n_actions;
  if (S <= 0 || A <= 0) {
    throw std::runtime_error("bellman: invalid dimensions");
  }
  if (static_cast<int>(input.v.size()) != S) {
    throw std::runtime_error("bellman: v has wrong size");
  }
  if (static_cast<int>(input.rewards.size()) != A * S ||
      static_cast<int>(input.transitions.size()) != A * S) {
    throw std::runtime_error("bellman: rewards/transitions have wrong size");
  }
  if (!input.sigma.empty() && static_cast<int>(input.sigma.size()) != A * S) {
    throw std::runtime_error("bellman: sigma has wrong size");
  }
  if (!projection) {
    throw std::runtime_error("bellman: projection callback is empty");
  }

  std::vector<std::vector<double>> b_actions(A, std::vector<double>(S, 0.0));
  double lower = input.lower_bound;
  double upper = input.upper_bound;
  double upper_candidate = -1.0e300;
  if (lower < 0.0) {
    lower = -1.0e300;
  }

  for (int a = 0; a < A; ++a) {
    double min_b = 1.0e300;
    double max_b = -1.0e300;
    for (int sp = 0; sp < S; ++sp) {
      double b = input.rewards[idx(a, sp, S)] + input.discount * input.v[sp];
      b_actions[a][sp] = b;
      min_b = std::min(min_b, b);
      max_b = std::max(max_b, b);
    }
    if (input.lower_bound < 0.0) {
      lower = std::max(lower, min_b);
    }
    if (input.upper_bound < 0.0) {
      upper_candidate = std::max(upper_candidate, max_b);
    }
  }

  if (input.upper_bound < 0.0) {
    upper = upper_candidate;
  }
  if (lower > upper) {
    throw std::runtime_error("bellman: invalid bounds");
  }

  std::vector<double> pbar(S, 0.0);
  std::vector<double> weights;
  if (!input.sigma.empty()) {
    weights.resize(S);
  }

  while (upper - lower > input.epsilon) {
    double theta = 0.5 * (lower + upper);
    double sum = 0.0;
    for (int a = 0; a < A; ++a) {
      for (int sp = 0; sp < S; ++sp) {
        pbar[sp] = input.transitions[idx(a, sp, S)];
        if (!weights.empty()) {
          weights[sp] = input.sigma[idx(a, sp, S)];
        }
      }
      ProjectionInput proj_input;
      proj_input.pbar = &pbar;
      proj_input.b = &b_actions[a];
      proj_input.weights = weights.empty() ? nullptr : &weights;
      proj_input.beta = theta;
      sum += projection(proj_input);
    }
    if (sum <= input.kappa) {
      upper = theta;
    } else {
      lower = theta;
    }
  }

  return upper;
}

} // namespace fast_bellman
