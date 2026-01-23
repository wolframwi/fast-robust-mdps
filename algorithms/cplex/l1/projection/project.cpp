#include "project.hpp"

#include <stdexcept>

#include <ilcplex/ilocplex.h>
ILOSTLBEGIN

namespace cplex_l1 {

ProjectionResult solve_projection_problem(const ProjectionInput& input, ::IloEnv& env) {
  const int n = static_cast<int>(input.pbar.size());
  if (static_cast<int>(input.b.size()) != n) {
    throw std::runtime_error("b and pbar must have the same length");
  }
  if (!input.weights.empty() && static_cast<int>(input.weights.size()) != n) {
    throw std::runtime_error("weights must be empty or have the same length as pbar");
  }

  try {
    IloModel model(env);
    IloNumVarArray p(env, n, 0.0, 1.0);
    IloNumVarArray t(env, n, 0.0, IloInfinity);

    IloExpr sum_p(env);
    for (int i = 0; i < n; ++i) {
      sum_p += p[i];
    }
    model.add(sum_p == 1.0);
    sum_p.end();

    IloExpr lhs(env);
    for (int i = 0; i < n; ++i) {
      lhs += input.b[i] * p[i];
    }
    model.add(lhs <= input.beta);
    lhs.end();

    for (int i = 0; i < n; ++i) {
      model.add(t[i] >= p[i] - input.pbar[i]);
      model.add(t[i] >= -p[i] + input.pbar[i]);
    }

    IloExpr obj(env);
    for (int i = 0; i < n; ++i) {
      double w = input.weights.empty() ? 1.0 : input.weights[i];
      obj += w * t[i];
    }
    model.add(IloMinimize(env, obj));
    obj.end();

    IloCplex cplex(model);
    cplex.setOut(env.getNullStream());
    cplex.setParam(IloCplex::Param::Threads, 1);
    if (!cplex.solve()) {
      throw std::runtime_error("CPLEX failed to solve L1 projection");
    }

    ProjectionResult result;
    result.p.resize(n);
    for (int i = 0; i < n; ++i) {
      result.p[i] = cplex.getValue(p[i]);
    }
    result.objective = cplex.getObjValue();
    return result;
  } catch (...) {
    throw;
  }
}

} // namespace cplex_l1
