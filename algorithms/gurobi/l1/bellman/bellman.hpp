#pragma once

#include <vector>

class GRBEnv;

namespace gurobi_l1_bellman {

struct BellmanInput {
  int n_states = 0;
  int n_actions = 0;
  double discount = 0.0;
  double kappa = 0.0;
  std::vector<double> v;           // size S
  std::vector<double> rewards;     // size A*S
  std::vector<double> transitions; // size A*S
  std::vector<double> sigma;       // size A*S (optional; if empty, weights=1)
};

double solve_bellman_state(const BellmanInput& input, ::GRBEnv& env);

} // namespace gurobi_l1_bellman
