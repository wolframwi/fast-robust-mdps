#include "project.hpp"

#include <stdexcept>
#include <string>

#include "fusion.h"
#include "monty.h"

using namespace mosek::fusion;
using namespace monty;

namespace mosek_l1 {

ProjectionResult solve_projection_problem(const ProjectionInput& input) {
  const int n = static_cast<int>(input.pbar.size());
  if (static_cast<int>(input.b.size()) != n) {
    throw std::runtime_error("b and pbar must have the same length");
  }
  if (!input.weights.empty() && static_cast<int>(input.weights.size()) != n) {
    throw std::runtime_error("weights must be empty or have the same length as pbar");
  }

  Model::t M = new Model("l1_projection");
  auto _M = finally([&]() { M->dispose(); });
  M->setSolverParam("numThreads", 1);
  auto p = M->variable("p", n, Domain::greaterThan(0.0));
  auto t = M->variable("t", n, Domain::greaterThan(0.0));

  auto b = new_array_ptr<double>(input.b);
  auto pbar = new_array_ptr<double>(input.pbar);

  M->constraint(Expr::sum(p), Domain::equalsTo(1.0));
  M->constraint(Expr::dot(b, p), Domain::lessThan(input.beta));

  M->constraint(Expr::sub(t, Expr::sub(p, pbar)), Domain::greaterThan(0.0));
  M->constraint(Expr::sub(t, Expr::sub(pbar, p)), Domain::greaterThan(0.0));

  std::vector<double> wvec = input.weights.empty() ? std::vector<double>(n, 1.0)
                                                    : input.weights;
  auto w = new_array_ptr<double>(wvec);
  M->objective(ObjectiveSense::Minimize, Expr::dot(w, t));

  M->solve();
  auto status = M->getPrimalSolutionStatus();
  auto pstatus = M->getProblemStatus();
  if (status != SolutionStatus::Optimal) {
    std::string msg = "MOSEK failed to solve L1 projection (sol="
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
  result.objective = M->primalObjValue();
  return result;
}

} // namespace mosek_l1
