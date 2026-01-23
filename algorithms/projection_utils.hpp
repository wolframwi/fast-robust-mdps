#pragma once

#include <cmath>
#include <vector>

namespace projection_utils {

inline double l1_distance(const std::vector<double>& p,
                          const std::vector<double>& pbar,
                          const std::vector<double>& weights) {
  double total = 0.0;
  for (size_t i = 0; i < p.size(); ++i) {
    total += weights[i] * std::abs(p[i] - pbar[i]);
  }
  return total;
}

inline double l2_distance(const std::vector<double>& p,
                          const std::vector<double>& pbar,
                          const std::vector<double>& weights) {
  double total = 0.0;
  for (size_t i = 0; i < p.size(); ++i) {
    double diff = p[i] - pbar[i];
    total += weights[i] * diff * diff;
  }
  return total;
}

inline double kl_divergence(const std::vector<double>& p,
                            const std::vector<double>& pbar) {
  double total = 0.0;
  for (size_t i = 0; i < p.size(); ++i) {
    if (p[i] > 0.0) {
      total += p[i] * (std::log(p[i]) - std::log(pbar[i]));
    }
  }
  return total;
}

inline double burg_divergence(const std::vector<double>& p,
                              const std::vector<double>& pbar) {
  double total = 0.0;
  for (size_t i = 0; i < p.size(); ++i) {
    total += pbar[i] * (std::log(pbar[i]) - std::log(p[i]));
  }
  return total;
}

} // namespace projection_utils
