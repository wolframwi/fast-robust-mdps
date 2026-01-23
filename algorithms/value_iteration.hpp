#pragma once

#include <algorithm>
#include <cmath>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace value_iteration {

struct Result {
  std::vector<double> value;
  int iterations = 0;
};

template <typename BellmanOperator>
inline Result solve(int n_states, const BellmanOperator& bellman,
                    double tolerance = 1e-4,
                    std::ostream* log = nullptr,
                    int report_every = 0,
                    int max_iterations = -1) {
  if (n_states <= 0) {
    throw std::runtime_error("value_iteration: invalid number of states");
  }
  if (tolerance <= 0.0) {
    throw std::runtime_error("value_iteration: tolerance must be positive");
  }

  std::vector<double> v(n_states, 0.0);
  std::vector<double> v_next(n_states, 0.0);
  double max_change = 0.0;
  int iterations = 0;

  do {
    max_change = 0.0;
    for (int s = 0; s < n_states; ++s) {
      v_next[s] = bellman(s, v);
      max_change = std::max(max_change, std::abs(v_next[s] - v[s]));
    }
    v.swap(v_next);
    ++iterations;
    if (log && (report_every <= 1 || iterations % report_every == 0)) {
      *log << "vi iter=" << iterations << " max_change=" << max_change << "\n";
    }
    if (max_iterations >= 0 && iterations >= max_iterations && max_change > tolerance) {
      throw std::runtime_error("value_iteration: maximum iterations reached");
    }
  } while (max_change > tolerance);

  if (log) {
    *log << "vi converged iter=" << iterations << " max_change=" << max_change << "\n";
  }

  return Result{v, iterations};
}

} // namespace value_iteration
