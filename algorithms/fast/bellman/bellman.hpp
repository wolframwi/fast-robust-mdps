#pragma once

#include <functional>
#include <vector>

namespace fast_bellman {

struct BellmanStateInput {
  int n_states = 0;
  int n_actions = 0;
  double discount = 0.0;
  double kappa = 0.0;
  double epsilon = 1e-6;
  double lower_bound = 0.0; // if negative, computed from rewards + discount*v
  double upper_bound = -1.0; // if negative, computed from rewards + discount*v
  std::vector<double> v;            // size S
  std::vector<double> rewards;      // size A*S, order (a, sp)
  std::vector<double> transitions;  // size A*S, order (a, sp)
  std::vector<double> sigma;        // optional size A*S (for L1/L2), empty otherwise
};

struct ProjectionInput {
  const std::vector<double>* pbar = nullptr;
  const std::vector<double>* b = nullptr;
  const std::vector<double>* weights = nullptr; // optional
  double beta = 0.0;
};

using ProjectionFn = std::function<double(const ProjectionInput&)>;

double solve_robust_bellman_state(const BellmanStateInput& input,
                                  const ProjectionFn& projection);

} // namespace fast_bellman
