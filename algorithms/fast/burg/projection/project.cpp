#include "project.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fast_burg {

namespace {

double burg_divergence(const std::vector<double>& p,
                       const std::vector<double>& pbar,
                       const std::vector<double>& log_pbar) {
  const int n = static_cast<int>(p.size());
  double div = 0.0;
  for (int i = 0; i < n; ++i) {
    div += pbar[i] * (log_pbar[i] - std::log(p[i]));
  }
  return div;
}

} // namespace

ProjectionResult solve_projection_problem(const ProjectionInput& input) {
  const int n = static_cast<int>(input.pbar.size());
  if (static_cast<int>(input.b.size()) != n) {
    throw std::runtime_error("fast_burg: b and pbar must have the same length");
  }
  if (n == 0) {
    throw std::runtime_error("fast_burg: empty input");
  }

  std::vector<double> log_pbar(n, 0.0);
  double sum_pbar = 0.0;
  double min_b = std::numeric_limits<double>::infinity();
  for (int i = 0; i < n; ++i) {
    if (input.pbar[i] <= 0.0) {
      throw std::runtime_error("fast_burg: pbar must be strictly positive");
    }
    log_pbar[i] = std::log(input.pbar[i]);
    sum_pbar += input.pbar[i];
    min_b = std::min(min_b, input.b[i]);
  }
  if (std::abs(sum_pbar - 1.0) > 1e-8) {
    throw std::runtime_error("fast_burg: pbar must sum to 1");
  }

  double baseline = 0.0;
  for (int i = 0; i < n; ++i) {
    baseline += input.pbar[i] * input.b[i];
  }
  if (baseline <= input.beta) {
    ProjectionResult result;
    result.p = input.pbar;
    result.objective = 0.0;
    return result;
  }

  if (input.beta <= min_b) {
    throw std::runtime_error("fast_burg: infeasible (beta <= min(b))");
  }

  const double tol = 1e-10;
  const double eps_dom = 1e-12;
  const double denom = input.beta - min_b;

  std::vector<double> d(n, 0.0);
  for (int i = 0; i < n; ++i) {
    d[i] = (input.b[i] - input.beta) / denom;
  }

  double alpha_L = 0.0;
  double alpha_U = 1.0 - eps_dom;

  auto psi_prime = [&](double alpha) {
    double value = 0.0;
    for (int i = 0; i < n; ++i) {
      double denom_i = 1.0 + alpha * d[i];
      if (denom_i <= 0.0) {
        throw std::runtime_error("fast_burg: invalid denominator in psi_prime");
      }
      value += input.pbar[i] * (d[i] / denom_i);
    }
    return value;
  };

  double psi_L = psi_prime(alpha_L);
  double psi_U = psi_prime(alpha_U);
  if (psi_L <= 0.0) {
    throw std::runtime_error("fast_burg: expected positive derivative at alpha=0");
  }
  if (psi_U >= 0.0) {
    throw std::runtime_error("fast_burg: failed to bracket alpha");
  }

  while (alpha_U - alpha_L > tol) {
    double alpha = 0.5 * (alpha_L + alpha_U);
    double val = psi_prime(alpha);
    if (val > 0.0) {
      alpha_L = alpha;
    } else {
      alpha_U = alpha;
    }
  }

  std::vector<double> p(n, 0.0);
  double sum_p = 0.0;
  for (int i = 0; i < n; ++i) {
    double denom_i = 1.0 + alpha_U * d[i];
    if (denom_i <= 0.0) {
      throw std::runtime_error("fast_burg: invalid denominator");
    }
    p[i] = input.pbar[i] / denom_i;
    sum_p += p[i];
  }
  if (sum_p <= 0.0) {
    throw std::runtime_error("fast_burg: invalid normalization");
  }
  if (std::abs(sum_p - 1.0) > 1e-12) {
    for (int i = 0; i < n; ++i) {
      p[i] /= sum_p;
    }
  }

  ProjectionResult result;
  result.p = std::move(p);
  result.objective = burg_divergence(result.p, input.pbar, log_pbar);
  return result;
}

} // namespace fast_burg
