#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "algorithms/mdp.hpp"
#include "algorithms/fast/burg/projection/project.hpp"

int main() {
  const std::string instance_dir =
      "../../../../instances/synthetic/baseline/S=50/A=10/rep=1";
  const std::string mdp_path = instance_dir + "/nominal.mdp";
  const std::string amb_path = instance_dir + "/moderate.amb";

  mdp::AmbiguousMDP data = mdp::load_mdp_amb(mdp_path, amb_path);
  const int S = data.mdp.n_states;
  const int A = data.mdp.n_actions;

  std::vector<double> transitions = data.mdp.transitions;
  const double bump = 1.0 / (10.0 * static_cast<double>(S));
  for (int s = 0; s < S; ++s) {
    for (int a = 0; a < A; ++a) {
      double sum = 0.0;
      for (int sp = 0; sp < S; ++sp) {
        int idx = mdp::index(s, a, sp, S, A);
        transitions[idx] += bump;
        sum += transitions[idx];
      }
      for (int sp = 0; sp < S; ++sp) {
        int idx = mdp::index(s, a, sp, S, A);
        transitions[idx] /= sum;
      }
    }
  }

  double r_max = 0.0;
  for (double r : data.mdp.rewards) {
    r_max = std::max(r_max, r);
  }
  if (r_max < 0.0) {
    r_max = 0.0;
  }
  double vmax = (data.mdp.discount >= 1.0) ? r_max : r_max / (1.0 - data.mdp.discount);

  std::mt19937 rng(0);
  std::uniform_real_distribution<double> dist(0.0, vmax);
  std::vector<double> V(S, 0.0);
  for (int i = 0; i < S; ++i) {
    V[i] = dist(rng);
  }
  double min_b = *std::min_element(V.begin(), V.end());

  const int repeats = 1000;
  const int total_calls = repeats * S * A;
  double objective_sum = 0.0;

  auto start = std::chrono::steady_clock::now();
  for (int rep = 0; rep < repeats; ++rep) {
    if ((rep + 1) % 100 == 0) {
      std::cout << "run " << (rep + 1) << "/" << repeats << "\n";
    }
    for (int s = 0; s < S; ++s) {
      for (int a = 0; a < A; ++a) {
        std::vector<double> pbar(S, 0.0);
        for (int sp = 0; sp < S; ++sp) {
          int idx = mdp::index(s, a, sp, S, A);
          pbar[sp] = transitions[idx];
        }

        double baseline = 0.0;
        for (int sp = 0; sp < S; ++sp) {
          baseline += pbar[sp] * V[sp];
        }
        double beta = baseline;
        if (baseline > min_b) {
          beta = 0.5 * (baseline + min_b);
        }

        fast_burg::ProjectionInput in_fast{pbar, V, beta};
        auto result = fast_burg::solve_projection_problem(in_fast);
        objective_sum += result.objective;
      }
    }
  }
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  double avg_seconds = elapsed.count() / static_cast<double>(total_calls);
  double avg_microseconds = avg_seconds * 1e6;

  std::cout << "Burg profiling done. objective_sum=" << objective_sum << "\n";
  std::cout << "Average time per projection: " << avg_microseconds << " us\n";
  return 0;
}
