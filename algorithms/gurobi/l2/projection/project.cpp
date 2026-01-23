#include "project.hpp"

#include <stdexcept>
#include <string>
#include <vector>

#include "gurobi_c++.h"

namespace gurobi_l2 {

ProjectionResult solve_projection_problem(const ProjectionInput& input, ::GRBEnv& env) {
  const int n = static_cast<int>(input.pbar.size());
  if (static_cast<int>(input.b.size()) != n) {
    throw std::runtime_error("b and pbar must have the same length");
  }
  if (!input.weights.empty() && static_cast<int>(input.weights.size()) != n) {
    throw std::runtime_error("weights must be empty or have the same length as pbar");
  }

  try {
    GRBModel model(env);

    std::vector<GRBVar> p;
    p.reserve(n);
    for (int i = 0; i < n; ++i) {
      p.push_back(model.addVar(0.0, 1.0, 0.0, GRB_CONTINUOUS));
    }

    GRBLinExpr sum_p = 0.0;
    for (int i = 0; i < n; ++i) {
      sum_p += p[i];
    }
    model.addConstr(sum_p == 1.0);

    GRBLinExpr lhs = 0.0;
    for (int i = 0; i < n; ++i) {
      lhs += input.b[i] * p[i];
    }
    model.addConstr(lhs <= input.beta);

    GRBQuadExpr obj = 0.0;
    for (int i = 0; i < n; ++i) {
      double w = input.weights.empty() ? 1.0 : input.weights[i];
      GRBLinExpr diff = p[i] - input.pbar[i];
      obj += w * diff * diff;
    }
    model.setObjective(obj, GRB_MINIMIZE);

    model.optimize();
    if (model.get(GRB_IntAttr_Status) != GRB_OPTIMAL) {
      throw std::runtime_error("Gurobi failed to solve L2 projection");
    }

    ProjectionResult result;
    result.p.resize(n);
    for (int i = 0; i < n; ++i) {
      result.p[i] = p[i].get(GRB_DoubleAttr_X);
    }
    result.objective = model.get(GRB_DoubleAttr_ObjVal);
    return result;
  } catch (const GRBException& exc) {
    throw std::runtime_error("Gurobi L2 projection error: " + exc.getMessage());
  }
}

} // namespace gurobi_l2
