#include "bellman.hpp"

#include <stdexcept>

#include <ilcplex/ilocplex.h>
#include <ilconcert/iloexpression.h>
ILOSTLBEGIN

namespace cplex_l2_bellman {

double solve_bellman_state(const BellmanInput& input, ::IloEnv& env) {
  const int S = input.n_states;
  const int A = input.n_actions;
  if (S <= 0 || A <= 0) {
    throw std::runtime_error("cplex_l2_bellman: invalid dimensions");
  }
  if (static_cast<int>(input.v.size()) != S ||
      static_cast<int>(input.rewards.size()) != A * S ||
      static_cast<int>(input.transitions.size()) != A * S) {
    throw std::runtime_error("cplex_l2_bellman: invalid input sizes");
  }
  if (!input.sigma.empty() && static_cast<int>(input.sigma.size()) != A * S) {
    throw std::runtime_error("cplex_l2_bellman: sigma has wrong size");
  }

  try {
    IloModel model(env);
    IloNumVar t(env, -IloInfinity, IloInfinity);

    IloArray<IloNumVarArray> p(env, A);
    IloNumVarArray eta(env, A, 0.0, IloInfinity);
    for (int a = 0; a < A; ++a) {
      p[a] = IloNumVarArray(env, S, 0.0, 1.0);
    }

    for (int a = 0; a < A; ++a) {
      IloExpr sum_p(env);
      for (int sp = 0; sp < S; ++sp) {
        sum_p += p[a][sp];
      }
      model.add(sum_p == 1.0);
      sum_p.end();
    }

    for (int a = 0; a < A; ++a) {
      IloExpr lhs(env);
      for (int sp = 0; sp < S; ++sp) {
        double z = input.rewards[a * S + sp] + input.discount * input.v[sp];
        lhs += p[a][sp] * z;
      }
      model.add(t >= lhs);
      lhs.end();
    }

    IloExpr budget(env);
    for (int a = 0; a < A; ++a) {
      IloExpr q(env);
      for (int sp = 0; sp < S; ++sp) {
        double pbar = input.transitions[a * S + sp];
        double w = input.sigma.empty() ? 1.0 : input.sigma[a * S + sp];
        IloExpr diff = p[a][sp] - pbar;
        q += w * w * diff * diff;
        diff.end();
      }
      model.add(q <= eta[a]);
      q.end();
      budget += eta[a];
    }
    model.add(budget <= input.kappa);
    budget.end();

    model.add(IloMinimize(env, t));

    IloCplex cplex(model);
    cplex.setOut(env.getNullStream());
    cplex.setParam(IloCplex::Param::Threads, 1);
    if (!cplex.solve()) {
      throw std::runtime_error("CPLEX failed to solve L2 Bellman");
    }

    double result = cplex.getObjValue();
    return result;
  } catch (...) {
    throw;
  }
}

} // namespace cplex_l2_bellman
