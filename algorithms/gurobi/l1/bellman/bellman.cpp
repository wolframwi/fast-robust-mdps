#include "bellman.hpp"

#include <stdexcept>
#include <string>
#include <vector>

#include "gurobi_c++.h"

namespace gurobi_l1_bellman {

double solve_bellman_state(const BellmanInput& input, ::GRBEnv& env) {
  const int S = input.n_states;
  const int A = input.n_actions;
  if (S <= 0 || A <= 0) {
    throw std::runtime_error("gurobi_l1_bellman: invalid dimensions");
  }
  if (static_cast<int>(input.v.size()) != S ||
      static_cast<int>(input.rewards.size()) != A * S ||
      static_cast<int>(input.transitions.size()) != A * S) {
    throw std::runtime_error("gurobi_l1_bellman: invalid input sizes");
  }
  if (!input.sigma.empty() && static_cast<int>(input.sigma.size()) != A * S) {
    throw std::runtime_error("gurobi_l1_bellman: sigma has wrong size");
  }

  std::vector<double> wvec = input.sigma.empty() ? std::vector<double>(A * S, 1.0)
                                                 : input.sigma;
  for (double w : wvec) {
    if (w < 0.0) {
      throw std::runtime_error("gurobi_l1_bellman: weights must be nonnegative");
    }
  }

  try {
    GRBModel model(env);

    GRBVar t = model.addVar(-GRB_INFINITY, GRB_INFINITY, 0.0, GRB_CONTINUOUS);

    std::vector<std::vector<GRBVar>> p(A, std::vector<GRBVar>(S));
    std::vector<std::vector<GRBVar>> y(A, std::vector<GRBVar>(S));
    for (int a = 0; a < A; ++a) {
      for (int sp = 0; sp < S; ++sp) {
        p[a][sp] = model.addVar(0.0, 1.0, 0.0, GRB_CONTINUOUS);
        y[a][sp] = model.addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS);
      }
    }

    for (int a = 0; a < A; ++a) {
      GRBLinExpr sum_p = 0.0;
      for (int sp = 0; sp < S; ++sp) {
        sum_p += p[a][sp];
      }
      model.addConstr(sum_p == 1.0);
    }

    for (int a = 0; a < A; ++a) {
      GRBLinExpr lhs = 0.0;
      for (int sp = 0; sp < S; ++sp) {
        double z = input.rewards[a * S + sp] + input.discount * input.v[sp];
        lhs += z * p[a][sp];
      }
      model.addConstr(t >= lhs);
    }

    GRBLinExpr budget = 0.0;
    for (int a = 0; a < A; ++a) {
      for (int sp = 0; sp < S; ++sp) {
        double pbar = input.transitions[a * S + sp];
        model.addConstr(y[a][sp] >= p[a][sp] - pbar);
        model.addConstr(y[a][sp] >= -p[a][sp] + pbar);
        budget += wvec[a * S + sp] * y[a][sp];
      }
    }
    model.addConstr(budget <= input.kappa);

    model.setObjective(GRBLinExpr(t), GRB_MINIMIZE);
    model.optimize();
    if (model.get(GRB_IntAttr_Status) != GRB_OPTIMAL) {
      throw std::runtime_error("Gurobi failed to solve L1 Bellman");
    }

    return model.get(GRB_DoubleAttr_ObjVal);
  } catch (const GRBException& exc) {
    throw std::runtime_error("Gurobi L1 Bellman error: " + exc.getMessage());
  }
}

} // namespace gurobi_l1_bellman
