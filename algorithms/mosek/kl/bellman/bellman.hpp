#pragma once

#include <vector>

namespace mosek_kl_bellman {

struct BellmanInput {
  int n_states = 0;
  int n_actions = 0;
  double discount = 0.0;
  double kappa = 0.0;
  std::vector<double> v;           // size S
  std::vector<double> rewards;     // size A*S
  std::vector<double> transitions; // size A*S
  std::vector<double> sigma;       // size A*S (unused)
};

double solve_bellman_state(const BellmanInput& input);

} // namespace mosek_kl_bellman
