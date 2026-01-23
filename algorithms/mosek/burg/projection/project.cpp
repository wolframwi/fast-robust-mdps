#include "project.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

#include "fusion.h"
#include "monty.h"

using namespace mosek::fusion;
using namespace monty;

namespace mosek_burg {

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

  Model::t M = new Model("burg_projection");
  auto _M = finally([&]() { M->dispose(); });
  M->setSolverParam("numThreads", 1);
  auto p = M->variable("p", n, Domain::greaterThan(0.0));
  auto s = M->variable("s", n, Domain::unbounded());

  auto b = new_array_ptr<double>(input.b);
  auto pbar = new_array_ptr<double>(input.pbar);

  M->constraint(Expr::sum(p), Domain::equalsTo(1.0));
  M->constraint(Expr::dot(b, p), Domain::lessThan(input.beta));

  for (int i = 0; i < n; ++i) {
    auto si = s->index(i);
    auto pi = p->index(i);
    auto one = Expr::constTerm(1.0);
    M->constraint(Expr::hstack(pi, one, Expr::neg(si)), Domain::inPExpCone());
  }

  M->objective(ObjectiveSense::Minimize, Expr::dot(pbar, s));

  M->solve();
  auto status = M->getPrimalSolutionStatus();
  auto pstatus = M->getProblemStatus();
  if (status != SolutionStatus::Optimal) {
    std::string msg = "MOSEK failed to solve Burg projection (sol="
                      + std::to_string(static_cast<int>(status))
                      + ", prob=" + std::to_string(static_cast<int>(pstatus)) + ")";
    throw std::runtime_error(msg);
  }

  ProjectionResult result;
  result.p.resize(n);
  auto psol = p->level();
  for (int i = 0; i < n; ++i) {
    result.p[i] = (*psol)[i];
  }
  double constant = 0.0;
  for (double v : input.pbar) {
    constant += v * std::log(v);
  }
  result.objective = M->primalObjValue() + constant;
  return result;
}

} // namespace mosek_burg
