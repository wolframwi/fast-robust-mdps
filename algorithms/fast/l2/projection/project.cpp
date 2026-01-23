#include "project.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace fast_l2 {

namespace {

double dot(const std::vector<double>& a, const std::vector<double>& b) {
  double total = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    total += a[i] * b[i];
  }
  return total;
}

bool solve_with_incremental_updates(const std::vector<double>& a,
                                    const std::vector<double>& b,
                                    const std::vector<double>& c,
                                    double rho,
                                    double target,
                                    double* alpha_out,
                                    double* gamma_out) {
  const double eps = 1e-12;
  const int n = static_cast<int>(b.size());
  if (n == 0) {
    return false;
  }

  std::vector<bool> active(n, false);
  double sum_a = 0.0;
  double sum_ab = 0.0;
  double sum_ac = 0.0;
  double sum_ab2 = 0.0;
  double sum_abc = 0.0;
  double b_min = b[0];
  for (int i = 0; i < n; ++i) {
    if (std::abs(b[i] - b_min) <= eps) {
      active[i] = true;
      sum_a += a[i];
      sum_ab += a[i] * b[i];
      sum_ac += a[i] * c[i];
      sum_ab2 += a[i] * b[i] * b[i];
      sum_abc += a[i] * b[i] * c[i];
    }
  }

  double alpha_prev = std::numeric_limits<double>::infinity();
  double last_m = b_min;
  double last_v = (rho - sum_ac) / sum_a;
  while (alpha_prev > eps) {
    double m = sum_ab / sum_a;
    double v = (rho - sum_ac) / sum_a;
    last_m = m;
    last_v = v;

    double alpha_next = 0.0;
    int next_idx = -1;
    for (int i = 0; i < n; ++i) {
      if (active[i]) {
        continue;
      }
      double denom = b[i] - m;
      if (denom <= eps) {
        continue;
      }
      double alpha = (v + c[i]) / denom;
      if (alpha <= eps || alpha >= alpha_prev - eps) {
        continue;
      }
      if (alpha > alpha_next) {
        alpha_next = alpha;
        next_idx = i;
      }
    }

    double low = alpha_next;
    double high = alpha_prev;

    double det = sum_a * sum_ab2 - sum_ab * sum_ab;
    if (std::abs(det) > 1e-14) {
      double rhs1 = rho - sum_ac;
      double rhs2 = target - sum_abc;
      double alpha = (sum_ab * rhs1 - sum_a * rhs2) / det;
      if (alpha >= low - 1e-10 && (!std::isfinite(high) || alpha <= high + 1e-10)) {
        if (alpha < low) {
          alpha = low;
        }
        if (std::isfinite(high) && alpha > high) {
          alpha = high;
        }
        *alpha_out = alpha;
        *gamma_out = m * alpha + v;
        return true;
      }
    }

    if (next_idx < 0) {
      break;
    }

    active[next_idx] = true;
    sum_a += a[next_idx];
    sum_ab += a[next_idx] * b[next_idx];
    sum_ac += a[next_idx] * c[next_idx];
    sum_ab2 += a[next_idx] * b[next_idx] * b[next_idx];
    sum_abc += a[next_idx] * b[next_idx] * c[next_idx];
    alpha_prev = alpha_next;
  }

  *alpha_out = 0.0;
  *gamma_out = last_m * (*alpha_out) + last_v;
  return false;
}

} // namespace

ProjectionResult solve_projection_problem(const ProjectionInput& input) {
  const int n = static_cast<int>(input.pbar.size());
  if (static_cast<int>(input.b.size()) != n) {
    throw std::runtime_error("fast_l2: b and pbar must have the same length");
  }
  if (!input.weights.empty() && static_cast<int>(input.weights.size()) != n) {
    throw std::runtime_error("fast_l2: weights must be empty or have the same length as pbar");
  }
  if (n == 0) {
    throw std::runtime_error("fast_l2: empty input");
  }

  std::vector<double> sigma = input.weights.empty()
                                  ? std::vector<double>(n, 1.0)
                                  : input.weights;
  std::vector<double> sigma_sq(n, 0.0);
  std::vector<double> inv_sigma_sq(n, 0.0);

  double sum_pbar = 0.0;
  double min_b = std::numeric_limits<double>::infinity();
  for (int i = 0; i < n; ++i) {
    if (input.pbar[i] < 0.0) {
      throw std::runtime_error("fast_l2: pbar must be non-negative");
    }
    if (sigma[i] <= 0.0) {
      throw std::runtime_error("fast_l2: weights must be positive");
    }
    sigma_sq[i] = sigma[i] * sigma[i];
    inv_sigma_sq[i] = 1.0 / sigma_sq[i];
    sum_pbar += input.pbar[i];
    min_b = std::min(min_b, input.b[i]);
  }
  if (std::abs(sum_pbar - 1.0) > 1e-8) {
    throw std::runtime_error("fast_l2: pbar must sum to 1");
  }
  if (input.beta < min_b - 1e-12) {
    throw std::runtime_error("fast_l2: infeasible (beta < min(b))");
  }

  const double baseline = dot(input.pbar, input.b);
  if (baseline <= input.beta + 1e-12) {
    ProjectionResult result;
    result.p = input.pbar;
    result.objective = 0.0;
    return result;
  }

  std::vector<int> order(n);
  for (int i = 0; i < n; ++i) {
    order[i] = i;
  }
  std::sort(order.begin(), order.end(), [&](int i, int j) {
    return input.b[i] < input.b[j];
  });

  std::vector<double> b_sorted(n, 0.0);
  std::vector<double> inv_sigma_sq_sorted(n, 0.0);
  std::vector<double> a_sorted(n, 0.0);
  std::vector<double> c_sorted(n, 0.0);

  for (int i = 0; i < n; ++i) {
    int idx = order[i];
    b_sorted[i] = input.b[idx];
    inv_sigma_sq_sorted[i] = inv_sigma_sq[idx];
    a_sorted[i] = inv_sigma_sq[idx];
    c_sorted[i] = 2.0 * sigma_sq[idx] * input.pbar[idx];
  }

  const double target = 2.0 * input.beta;
  double alpha = 0.0;
  double gamma = 0.0;
  solve_with_incremental_updates(a_sorted, b_sorted, c_sorted, 2.0, target, &alpha, &gamma);

  ProjectionResult result;
  result.p.resize(n, 0.0);
  result.objective = 0.0;
  for (int i = 0; i < n; ++i) {
    double term = -input.b[i] * alpha + gamma + 2.0 * sigma_sq[i] * input.pbar[i];
    double p = term > 0.0 ? 0.5 * inv_sigma_sq[i] * term : 0.0;
    result.p[i] = p;
    double diff = p - input.pbar[i];
    result.objective += sigma_sq[i] * diff * diff;
  }

  return result;
}

} // namespace fast_l2
