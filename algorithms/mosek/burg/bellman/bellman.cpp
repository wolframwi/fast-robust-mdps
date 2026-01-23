#include "bellman.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

#include "fusion.h"
#include "monty.h"

using namespace mosek::fusion;
using namespace monty;

namespace mosek_burg_bellman {

double solve_bellman_state(const BellmanInput& input) {
  const int S = input.n_states;
  const int A = input.n_actions;
  if (S <= 0 || A <= 0) {
    throw std::runtime_error("mosek_burg_bellman: invalid dimensions");
  }
  if (static_cast<int>(input.v.size()) != S ||
      static_cast<int>(input.rewards.size()) != A * S ||
      static_cast<int>(input.transitions.size()) != A * S) {
    throw std::runtime_error("mosek_burg_bellman: invalid input sizes");
  }

  double constant = 0.0;
  for (double v : input.transitions) {
    if (v <= 0.0) {
      throw std::runtime_error("mosek_burg_bellman: transitions must be positive");
    }
    constant += v * std::log(v);
  }
  double c_budget = constant - input.kappa;

  Model::t M = new Model("burg_bellman");
  auto _M = finally([&]() { M->dispose(); });
  M->setSolverParam("numThreads", 1);

  const int N = A * S;
  auto p = M->variable("p", N, Domain::greaterThan(0.0));
  auto s = M->variable("s", N, Domain::unbounded());
  auto t = M->variable("t", 1, Domain::unbounded());

  for (int a = 0; a < A; ++a) {
    int start = a * S;
    int end = (a + 1) * S;
    M->constraint(Expr::sum(p->slice(start, end)), Domain::equalsTo(1.0));
  }

  for (int a = 0; a < A; ++a) {
    std::vector<double> z(S, 0.0);
    for (int sp = 0; sp < S; ++sp) {
      z[sp] = input.rewards[a * S + sp] + input.discount * input.v[sp];
    }
    int start = a * S;
    int end = (a + 1) * S;
    auto zptr = new_array_ptr<double>(z);
    M->constraint(Expr::sub(Expr::dot(zptr, p->slice(start, end)), t->index(0)),
                  Domain::lessThan(0.0));
  }

  for (int i = 0; i < N; ++i) {
    auto si = s->index(i);
    auto pi = p->index(i);
    auto one = Expr::constTerm(1.0);
    M->constraint(Expr::hstack(pi, one, Expr::neg(si)), Domain::inPExpCone());
  }

  auto pbar = new_array_ptr<double>(input.transitions);
  M->constraint(Expr::dot(pbar, s), Domain::lessThan(-c_budget));
  M->objective(ObjectiveSense::Minimize, Expr::sum(t));

  M->solve();
  auto status = M->getPrimalSolutionStatus();
  auto pstatus = M->getProblemStatus();
  if (status != SolutionStatus::Optimal) {
    std::string msg = "MOSEK failed to solve Burg Bellman (sol="
                      + std::to_string(static_cast<int>(status))
                      + ", prob=" + std::to_string(static_cast<int>(pstatus)) + ")";
    throw std::runtime_error(msg);
  }

  return M->primalObjValue();
}

} // namespace mosek_burg_bellman
