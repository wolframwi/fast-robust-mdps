#pragma once

#include <vector>

namespace mosek_l2 {

struct ProjectionInput {
  std::vector<double> pbar;
  std::vector<double> b;
  std::vector<double> weights; // optional; if empty, weights=1
  double beta = 0.0;
};

struct ProjectionResult {
  std::vector<double> p;
  double objective = 0.0;
};

ProjectionResult solve_projection_problem(const ProjectionInput& input);

} // namespace mosek_l2
