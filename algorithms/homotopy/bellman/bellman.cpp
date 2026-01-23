#include "bellman.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace homotopy_l1_bellman {

namespace {

struct Segment {
  double x0 = 0.0;
  double x1 = 0.0;
  double q0 = 0.0;
  double slope = 0.0;
};

struct ActionCurve {
  std::vector<Segment> segments;
  double q0 = 0.0;
  double q_min = 0.0;
};

int idx(int a, int sp, int n_states) {
  return a * n_states + sp;
}

double dot(const std::vector<double>& a, const std::vector<double>& b) {
  double total = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    total += a[i] * b[i];
  }
  return total;
}

double compute_delta(const std::vector<double>& p,
                     const std::vector<double>& pbar,
                     int i,
                     int j,
                     double dp_i,
                     double dp_j) {
  const double tol = 1e-12;
  double delta = std::numeric_limits<double>::infinity();

  if (dp_i < 0.0) {
    delta = std::min(delta, p[i] / (-dp_i));
  } else if (dp_i > 0.0) {
    delta = std::min(delta, (1.0 - p[i]) / dp_i);
  }

  if (dp_j < 0.0) {
    delta = std::min(delta, p[j] / (-dp_j));
  } else if (dp_j > 0.0) {
    delta = std::min(delta, (1.0 - p[j]) / dp_j);
  }

  if (p[i] > pbar[i] + tol && dp_i < 0.0) {
    delta = std::min(delta, (p[i] - pbar[i]) / (-dp_i));
  } else if (p[i] < pbar[i] - tol && dp_i > 0.0) {
    delta = std::min(delta, (pbar[i] - p[i]) / dp_i);
  }

  if (p[j] > pbar[j] + tol && dp_j < 0.0) {
    delta = std::min(delta, (p[j] - pbar[j]) / (-dp_j));
  } else if (p[j] < pbar[j] - tol && dp_j > 0.0) {
    delta = std::min(delta, (pbar[j] - p[j]) / dp_j);
  }

  if (!std::isfinite(delta) || delta <= tol) {
    return 0.0;
  }
  return delta;
}

ActionCurve build_curve(const std::vector<double>& pbar,
                        const std::vector<double>& z,
                        const std::vector<double>& weights) {
  const int S = static_cast<int>(pbar.size());
  if (static_cast<int>(z.size()) != S || static_cast<int>(weights.size()) != S) {
    throw std::runtime_error("homotopy_l1_bellman: invalid action sizes");
  }

  const double tol = 1e-12;
  std::vector<double> p = pbar;
  double xi = 0.0;
  double q = dot(p, z);
  ActionCurve curve;
  curve.q0 = q;

  const int max_steps = 10000 + S * S;
  for (int step = 0; step < max_steps; ++step) {
    double best_slope = std::numeric_limits<double>::infinity();
    int best_i = -1;
    int best_j = -1;
    double best_dp_i = 0.0;
    double best_dp_j = 0.0;

    for (int i = 0; i < S; ++i) {
      if (p[i] <= tol) {
        continue;
      }
      for (int j = 0; j < S; ++j) {
        if (i == j) {
          continue;
        }
        if (p[j] >= 1.0 - tol) {
          continue;
        }

        // Case C1: cross-nominal transfer.
        if (p[i] <= pbar[i] + tol && p[j] >= pbar[j] - tol) {
          double denom = weights[i] + weights[j];
          double dp_i = -1.0 / denom;
          double dp_j = 1.0 / denom;
          double slope = (z[j] - z[i]) / denom;
          double delta = compute_delta(p, pbar, i, j, dp_i, dp_j);
          if (delta > 0.0 && slope < best_slope) {
            best_slope = slope;
            best_i = i;
            best_j = j;
            best_dp_i = dp_i;
            best_dp_j = dp_j;
          }
        }

        // Case C2: same-side transfer above nominal with weight ordering.
        if (p[i] >= pbar[i] - tol && p[j] >= pbar[j] - tol &&
            weights[i] > weights[j] + tol) {
          double denom = weights[i] - weights[j];
          double dp_i = -1.0 / denom;
          double dp_j = 1.0 / denom;
          double slope = (z[j] - z[i]) / (-denom);
          double delta = compute_delta(p, pbar, i, j, dp_i, dp_j);
          if (delta > 0.0 && slope < best_slope) {
            best_slope = slope;
            best_i = i;
            best_j = j;
            best_dp_i = dp_i;
            best_dp_j = dp_j;
          }
        }
      }
    }

    if (!std::isfinite(best_slope) || best_slope >= -1e-14) {
      break;
    }

    double delta = compute_delta(p, pbar, best_i, best_j, best_dp_i, best_dp_j);
    if (delta <= tol) {
      break;
    }

    curve.segments.push_back({xi, xi + delta, q, best_slope});
    p[best_i] += best_dp_i * delta;
    p[best_j] += best_dp_j * delta;
    p[best_i] = std::min(1.0, std::max(0.0, p[best_i]));
    p[best_j] = std::min(1.0, std::max(0.0, p[best_j]));
    xi += delta;
    q += best_slope * delta;
  }

  curve.q_min = q;
  curve.segments.push_back(
      {xi, std::numeric_limits<double>::infinity(), q, 0.0});
  return curve;
}

double inverse_from_curve(const ActionCurve& curve, double u) {
  const double tol = 1e-12;
  if (u >= curve.q0 - tol) {
    return 0.0;
  }
  if (u < curve.q_min - tol) {
    return std::numeric_limits<double>::infinity();
  }
  for (const auto& seg : curve.segments) {
    double q1 = seg.q0;
    if (std::isfinite(seg.x1) && std::abs(seg.slope) > tol) {
      q1 = seg.q0 + seg.slope * (seg.x1 - seg.x0);
    }
    if (u >= q1 - tol) {
      if (std::abs(seg.slope) <= tol) {
        return seg.x0;
      }
      double xi = seg.x0 + (u - seg.q0) / seg.slope;
      if (xi < seg.x0) {
        xi = seg.x0;
      }
      if (std::isfinite(seg.x1) && xi > seg.x1) {
        xi = seg.x1;
      }
      return xi;
    }
  }
  return std::numeric_limits<double>::infinity();
}

} // namespace

double solve_bellman_state(const BellmanInput& input) {
  const int S = input.n_states;
  const int A = input.n_actions;
  if (S <= 0 || A <= 0) {
    throw std::runtime_error("homotopy_l1_bellman: invalid dimensions");
  }
  if (static_cast<int>(input.v.size()) != S ||
      static_cast<int>(input.rewards.size()) != A * S ||
      static_cast<int>(input.transitions.size()) != A * S) {
    throw std::runtime_error("homotopy_l1_bellman: invalid input sizes");
  }
  if (!input.sigma.empty() && static_cast<int>(input.sigma.size()) != A * S) {
    throw std::runtime_error("homotopy_l1_bellman: sigma has wrong size");
  }
  if (input.kappa < 0.0) {
    throw std::runtime_error("homotopy_l1_bellman: kappa must be nonnegative");
  }

  std::vector<ActionCurve> curves;
  curves.reserve(static_cast<size_t>(A));

  double upper = -std::numeric_limits<double>::infinity();
  double lower = std::numeric_limits<double>::infinity();

  for (int a = 0; a < A; ++a) {
    std::vector<double> pbar(S, 0.0);
    std::vector<double> weights(S, 1.0);
    std::vector<double> z(S, 0.0);
    for (int sp = 0; sp < S; ++sp) {
      pbar[sp] = input.transitions[idx(a, sp, S)];
      if (!input.sigma.empty()) {
        weights[sp] = input.sigma[idx(a, sp, S)];
        if (weights[sp] <= 0.0) {
          throw std::runtime_error("homotopy_l1_bellman: weights must be positive");
        }
      }
      z[sp] = input.rewards[idx(a, sp, S)] + input.discount * input.v[sp];
    }

    ActionCurve curve = build_curve(pbar, z, weights);
    upper = std::max(upper, curve.q0);
    lower = std::min(lower, curve.q_min);
    curves.push_back(std::move(curve));
  }

  double sum_lower = 0.0;
  for (const auto& curve : curves) {
    double xi = inverse_from_curve(curve, lower);
    if (!std::isfinite(xi)) {
      sum_lower = std::numeric_limits<double>::infinity();
      break;
    }
    sum_lower += xi;
  }
  if (sum_lower <= input.kappa) {
    return lower;
  }

  const double tol = 1e-6;
  for (int iter = 0; iter < 200 && upper - lower > tol; ++iter) {
    double mid = 0.5 * (lower + upper);
    double sum = 0.0;
    for (const auto& curve : curves) {
      double xi = inverse_from_curve(curve, mid);
      if (!std::isfinite(xi)) {
        sum = std::numeric_limits<double>::infinity();
        break;
      }
      sum += xi;
    }
    if (sum <= input.kappa) {
      upper = mid;
    } else {
      lower = mid;
    }
  }

  return upper;
}

} // namespace homotopy_l1_bellman
