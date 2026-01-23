#include "project.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fast_kl {

namespace {

// Uses precomputed log_pbar for efficiency.
void compute_p_and_expectation(const std::vector<double>& log_pbar,
                               const std::vector<double>& b,
                               double alpha,
                               std::vector<double>& p,
                               double& expectation) {
  const int n = static_cast<int>(log_pbar.size());
  double umax = -std::numeric_limits<double>::infinity();
  
  // First pass: find max for stability
  for (int i = 0; i < n; ++i) {
    double u = log_pbar[i] - alpha * b[i];
    if (u > umax) umax = u;
  }

  double sumw = 0.0;
  double sumwb = 0.0;
  // Second pass: compute weights and sum
  for (int i = 0; i < n; ++i) {
    double u = log_pbar[i] - alpha * b[i];
    double w = std::exp(u - umax);
    p[i] = w;
    sumw += w;
    sumwb += w * b[i];
  }

  expectation = sumwb / sumw;
  // Third pass: normalize
  double inv_sumw = 1.0 / sumw;
  for (int i = 0; i < n; ++i) {
    p[i] *= inv_sumw;
  }
}

// Robust KL divergence calculation handling p[i] == 0
double kl_divergence(const std::vector<double>& p,
                     const std::vector<double>& log_pbar) {
  const int n = static_cast<int>(p.size());
  double kl = 0.0;
  for (int i = 0; i < n; ++i) {
    if (p[i] > 0.0) {
      kl += p[i] * (std::log(p[i]) - log_pbar[i]);
    }
  }
  return kl;
}

} // namespace

ProjectionResult solve_projection_problem(const ProjectionInput& input) {
  const int n = static_cast<int>(input.pbar.size());
  if (static_cast<int>(input.b.size()) != n) {
    throw std::runtime_error("fast_kl: b and pbar must have the same length");
  }

  if (n == 0) {
    throw std::runtime_error("fast_kl: empty input");
  }

  // Precompute log_pbar and validate input
  std::vector<double> log_pbar(n);
  double sum_pbar = 0.0;
  double min_pbar = std::numeric_limits<double>::infinity();
  double min_b = std::numeric_limits<double>::infinity();
  
  for (int i = 0; i < n; ++i) {
    if (input.pbar[i] <= 0.0) {
      throw std::runtime_error("fast_kl: pbar must be strictly positive");
    }
    log_pbar[i] = std::log(input.pbar[i]);
    sum_pbar += input.pbar[i];
    if (input.pbar[i] < min_pbar) min_pbar = input.pbar[i];
    if (input.b[i] < min_b) min_b = input.b[i];
  }

  if (std::abs(sum_pbar - 1.0) > 1e-8) {
    throw std::runtime_error("fast_kl: pbar must sum to 1");
  }

  double baseline = 0.0;
  for (int i = 0; i < n; ++i) {
    baseline += input.pbar[i] * input.b[i];
  }
  
  // Case 1: Constraint already satisfied
  if (baseline <= input.beta) {
    ProjectionResult result;
    result.p = input.pbar;
    result.objective = 0.0;
    return result;
  }

  // Case 2: Infeasible
  if (input.beta <= min_b) {
    throw std::runtime_error("fast_kl: infeasible (beta <= min(b))");
  }

  const double tol_h = 1e-10 * std::max(1.0, std::max(std::abs(input.beta), std::abs(baseline)));
  const double alpha_upper = std::log(1.0 / min_pbar) / (input.beta - min_b);

  double alpha_L = 0.0;
  double alpha_U = alpha_upper;

  std::vector<double> p(n, 0.0);
  double expectation = 0.0;

  // Validate bracket
  compute_p_and_expectation(log_pbar, input.b, alpha_U, p, expectation);
  if (expectation > input.beta + tol_h) {
    throw std::runtime_error("fast_kl: failed to bracket alpha");
  }

  const double tol_alpha = 1e-12 * std::max(1.0, alpha_upper);

  while (true) {
    double alpha = 0.5 * (alpha_L + alpha_U);
    compute_p_and_expectation(log_pbar, input.b, alpha, p, expectation);
    
    double h = expectation - input.beta;
    
    // Converged by residual
    if (std::abs(h) <= tol_h) {
      alpha_U = alpha;
      break;
    }
    
    if (h > 0.0) {
      alpha_L = alpha;
    } else {
      alpha_U = alpha;
    }
    
    // Converged by interval width
    if (alpha_U - alpha_L <= tol_alpha) {
      break;
    }
  }

  // Recompute p for the final alpha
  compute_p_and_expectation(log_pbar, input.b, alpha_U, p, expectation);

  ProjectionResult result;
  result.p = p;
  result.objective = kl_divergence(result.p, log_pbar);
  return result;
}

} // namespace fast_kl
