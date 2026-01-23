#include "project.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "gurobi_c++.h"

namespace gurobi_kl {

ProjectionResult solve_projection_problem(const ProjectionInput& input) {
  const int n = static_cast<int>(input.pbar.size());
  if (static_cast<int>(input.b.size()) != n) {
    throw std::runtime_error("b and pbar must have the same length");
  }
  for (double v : input.pbar) {
    if (v <= 0.0) {
      throw std::runtime_error("pbar must be strictly positive");
    }
  }

  try {
    GRBEnv env(true);
    env.set(GRB_IntParam_OutputFlag, 0);
    env.set(GRB_IntParam_Threads, 1);
    env.start();
    GRBModel model(env);
    model.set(GRB_IntParam_NonConvex, 2);

    const double lb = 1e-12;
    std::vector<GRBVar> p;
    std::vector<GRBVar> y;
    std::vector<GRBVar> t;
    p.reserve(n);
    y.reserve(n);
    t.reserve(n);
    for (int i = 0; i < n; ++i) {
      p.push_back(model.addVar(lb, 1.0, 0.0, GRB_CONTINUOUS));
      y.push_back(model.addVar(-GRB_INFINITY, GRB_INFINITY, 0.0, GRB_CONTINUOUS));
      t.push_back(model.addVar(-GRB_INFINITY, GRB_INFINITY, 0.0, GRB_CONTINUOUS));
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

    for (int i = 0; i < n; ++i) {
      model.addGenConstrLog(p[i], y[i]);
      GRBQuadExpr prod = p[i] * y[i] - t[i];
      model.addQConstr(prod == 0.0);
    }

    GRBLinExpr obj = 0.0;
    for (int i = 0; i < n; ++i) {
      obj += t[i];
      obj += -p[i] * std::log(input.pbar[i]);
    }
    model.setObjective(obj, GRB_MINIMIZE);

    model.optimize();
    if (model.get(GRB_IntAttr_Status) != GRB_OPTIMAL) {
      throw std::runtime_error("Gurobi failed to solve KL projection");
    }

    ProjectionResult result;
    result.p.resize(n);
    for (int i = 0; i < n; ++i) {
      result.p[i] = p[i].get(GRB_DoubleAttr_X);
    }
    result.objective = model.get(GRB_DoubleAttr_ObjVal);
    return result;
  } catch (const GRBException& exc) {
    throw std::runtime_error("Gurobi KL projection error: " + exc.getMessage());
  }
}

} // namespace gurobi_kl
