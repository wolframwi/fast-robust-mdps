#pragma once

#include <vector>

namespace mosek_burg {

struct ProjectionInput {
  std::vector<double> pbar;
  std::vector<double> b;
  double beta = 0.0;
};

struct ProjectionResult {
  std::vector<double> p;
  double objective = 0.0;
};

ProjectionResult solve_projection_problem(const ProjectionInput& input);

} // namespace mosek_burg
