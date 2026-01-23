#include "project.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace fast_l1 {

namespace {

double dot(const std::vector<double>& a, const std::vector<double>& b) {
  double total = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    total += a[i] * b[i];
  }
  return total;
}

double line_intersection(double b1, double s1, double b2, double s2) {
  double denom = b1 - b2;
  double scale = std::max(1.0, std::max(std::abs(b1), std::abs(b2)));
  if (std::abs(denom) <= 1e-14 * scale) {
    if (s2 > s1) {
      return std::numeric_limits<double>::infinity();
    }
    if (s2 < s1) {
      return -std::numeric_limits<double>::infinity();
    }
    return -std::numeric_limits<double>::infinity();
  }
  return (s2 - s1) / denom;
}

double find_root_bisect(int i,
                        const std::vector<int>& env_idx,
                        const std::vector<double>& env_alpha,
                        const std::vector<double>& b,
                        const std::vector<double>& sigma) {
  const int env_n = static_cast<int>(env_idx.size());
  if (env_n == 1) {
    int j = env_idx[0];
    double denom = b[i] - b[j];
    if (denom > 0.0) {
      return (sigma[i] + sigma[j]) / denom;
    }
    return std::numeric_limits<double>::infinity();
  }

  int j1 = env_idx.front();
  int jn = env_idx.back();
  double alpha2 = env_alpha[1];
  double alphan = env_alpha.back();
  if (b[i] == b[jn]) {
    return std::numeric_limits<double>::infinity();
  }
  double denom_last = b[i] - b[jn];
  if (denom_last > 0.0 && denom_last * alphan <= sigma[i] + sigma[jn]) {
    return (sigma[i] + sigma[jn]) / denom_last;
  }
  double denom_first = b[i] - b[j1];
  if (denom_first > 0.0 && denom_first * alpha2 >= sigma[i] + sigma[j1]) {
    return (sigma[i] + sigma[j1]) / denom_first;
  }

  int left = 1;
  int right = env_n - 2;
  while (left <= right) {
    int mid = left + (right - left) / 2;
    int jm = env_idx[mid];
    double denom = b[i] - b[jm];
    if (denom <= 0.0) {
      right = mid - 1;
      continue;
    }
    double lhs = denom * env_alpha[mid];
    double rhs = sigma[i] + sigma[jm];
    if (lhs > rhs) {
      right = mid - 1;
      continue;
    }
    double rhs_next = denom * env_alpha[mid + 1];
    if (rhs_next < rhs) {
      left = mid + 1;
      continue;
    }
    return (sigma[i] + sigma[jm]) / denom;
  }

  return std::numeric_limits<double>::infinity();
}

bool alpha_equal(double a, double b) {
  const double tol = 1e-12;
  double scale = std::max(1.0, std::max(std::abs(a), std::abs(b)));
  return std::abs(a - b) <= tol * scale;
}

ProjectionResult solve_impl(const ProjectionInput& input) {
  const int n = static_cast<int>(input.pbar.size());
  if (static_cast<int>(input.b.size()) != n) {
    throw std::runtime_error("fast_l1: b and pbar must have the same length");
  }
  if (!input.weights.empty() && static_cast<int>(input.weights.size()) != n) {
    throw std::runtime_error("fast_l1: weights must be empty or have the same length as pbar");
  }
  if (n == 0) {
    throw std::runtime_error("fast_l1: empty input");
  }

  std::vector<double> sigma = input.weights.empty()
                                  ? std::vector<double>(n, 1.0)
                                  : input.weights;

  double sum_pbar = 0.0;
  double min_b = std::numeric_limits<double>::infinity();
  for (int i = 0; i < n; ++i) {
    if (input.pbar[i] < 0.0) {
      throw std::runtime_error("fast_l1: pbar must be non-negative");
    }
    if (sigma[i] <= 0.0) {
      throw std::runtime_error("fast_l1: weights must be positive");
    }
    sum_pbar += input.pbar[i];
    min_b = std::min(min_b, input.b[i]);
  }
  if (std::abs(sum_pbar - 1.0) > 1e-8) {
    throw std::runtime_error("fast_l1: pbar must sum to 1");
  }

  const double baseline = dot(input.pbar, input.b);
  if (baseline <= input.beta) {
    ProjectionResult result;
    result.objective = 0.0;
    return result;
  }
  if (input.beta < min_b) {
    throw std::runtime_error("fast_l1: infeasible (beta < min(b))");
  }

  // Step 1: sort by slope and remove redundant equal-slope lines.
  std::vector<int> indices(n);
  for (int i = 0; i < n; ++i) {
    indices[i] = i;
  }
  std::sort(indices.begin(), indices.end(), [&](int i, int j) {
    if (input.b[i] == input.b[j]) {
      return sigma[i] < sigma[j];
    }
    return input.b[i] > input.b[j];
  });

  std::vector<int> uniq;
  uniq.reserve(n);
  const double slope_tol = 1e-12;
  for (int idx : indices) {
    if (uniq.empty()) {
      uniq.push_back(idx);
      continue;
    }
    int last = uniq.back();
    if (std::abs(input.b[idx] - input.b[last]) <= slope_tol) {
      if (sigma[idx] < sigma[last]) {
        uniq.back() = idx;
      }
    } else {
      uniq.push_back(idx);
    }
  }

  if (uniq.empty()) {
    throw std::runtime_error("fast_l1: no supporting lines after filtering");
  }

  // Step 1b: dominance pruning for alpha >= 0.
  std::vector<int> filtered;
  filtered.reserve(uniq.size());
  double sigma_min = std::numeric_limits<double>::infinity();
  const double prune_eps = 1e-12;
  for (auto it = uniq.rbegin(); it != uniq.rend(); ++it) {
    int idx = *it;
    if (sigma[idx] >= sigma_min - prune_eps) {
      continue;
    }
    sigma_min = sigma[idx];
    filtered.push_back(idx);
  }
  std::reverse(filtered.begin(), filtered.end());
  if (filtered.empty()) {
    throw std::runtime_error("fast_l1: no supporting lines after pruning");
  }

  // Step 2: dual Graham scan to build the envelope.
  std::vector<int> env_idx;
  std::vector<double> env_alpha;
  env_idx.reserve(filtered.size());
  env_alpha.reserve(filtered.size());

  env_idx.push_back(filtered[0]);
  env_alpha.push_back(-std::numeric_limits<double>::infinity());

  if (filtered.size() > 1) {
    int idx2 = filtered[1];
    double alpha2 = line_intersection(input.b[env_idx.back()], sigma[env_idx.back()],
                                      input.b[idx2], sigma[idx2]);
    env_idx.push_back(idx2);
    env_alpha.push_back(alpha2);
  }

  for (size_t k = 2; k < filtered.size(); ++k) {
    int idx = filtered[k];
    while (true) {
      int top_idx = env_idx.back();
      double alpha = line_intersection(input.b[top_idx], sigma[top_idx],
                                       input.b[idx], sigma[idx]);
      if (env_idx.size() == 1 || alpha > env_alpha.back()) {
        env_idx.push_back(idx);
        env_alpha.push_back(alpha);
        break;
      }
      env_idx.pop_back();
      env_alpha.pop_back();
    }
  }

  // Remove line segments with negative right endpoints.
  size_t start = 0;
  while (start + 1 < env_idx.size() && env_alpha[start + 1] < 0.0) {
    start += 1;
  }
  if (start > 0) {
    env_idx.erase(env_idx.begin(), env_idx.begin() + static_cast<long>(start));
    env_alpha.erase(env_alpha.begin(), env_alpha.begin() + static_cast<long>(start));
  }

  if (env_idx.empty()) {
    throw std::runtime_error("fast_l1: empty envelope after trimming");
  }

  // Step 3: compute plus-term roots.
  std::vector<double> roots(n, std::numeric_limits<double>::infinity());
  for (int i = 0; i < n; ++i) {
    roots[i] = find_root_bisect(i, env_idx, env_alpha, input.b, sigma);
  }

  std::vector<std::pair<double, int>> root_events;
  root_events.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    if (std::isfinite(roots[i]) && roots[i] >= 0.0) {
      root_events.emplace_back(roots[i], i);
    }
  }
  std::sort(root_events.begin(), root_events.end(),
            [](const std::pair<double, int>& a, const std::pair<double, int>& b) {
              return a.first < b.first;
            });

  std::vector<int> env_breaks;
  env_breaks.reserve(env_idx.size());
  for (size_t k = 1; k < env_idx.size(); ++k) {
    if (env_alpha[k] >= 0.0 && std::isfinite(env_alpha[k])) {
      env_breaks.push_back(static_cast<int>(k));
    }
  }

  // Step 4: slope-crossing sweep over roots/envelope breakpoints.
  double alpha = 0.0;
  double f = 0.0;
  double grad_f1 = baseline - input.beta;
  double grad_f2 = 0.0;
  double grad_fmin = input.b[env_idx[0]];
  double pbar_sum = 0.0;
  int ell = 0;

  size_t i = 0;
  size_t j = 0;
  while (true) {
    double slope = grad_f1 - grad_f2;
    if (slope <= 0.0) {
      ProjectionResult result;
      result.objective = f;
      return result;
    }

    double next_env = (i < env_breaks.size())
                          ? env_alpha[env_breaks[i]]
                          : std::numeric_limits<double>::infinity();
    double next_root = (j < root_events.size())
                           ? root_events[j].first
                           : std::numeric_limits<double>::infinity();
    double next = std::min(next_env, next_root);
    if (!std::isfinite(next)) {
      break;
    }

    if (next > alpha) {
      f += slope * (next - alpha);
      alpha = next;
    }

    while (j < root_events.size() && alpha_equal(root_events[j].first, alpha)) {
      int idx = root_events[j].second;
      grad_f2 += input.pbar[idx] * (input.b[idx] - grad_fmin);
      pbar_sum += input.pbar[idx];
      ++j;
    }

    while (i < env_breaks.size() && alpha_equal(env_alpha[env_breaks[i]], alpha)) {
      ell = env_breaks[i];
      double next_grad = input.b[env_idx[ell]];
      grad_f2 -= (next_grad - grad_fmin) * pbar_sum;
      grad_fmin = next_grad;
      ++i;
    }
  }

  double slope = grad_f1 - grad_f2;
  if (slope > 0.0) {
    throw std::runtime_error("fast_l1: unexpected positive slope at infinity");
  }

  ProjectionResult result;
  result.objective = f;
  return result;
}

} // namespace

ProjectionResult solve_projection_problem(const ProjectionInput& input) {
  return solve_impl(input);
}

} // namespace fast_l1
