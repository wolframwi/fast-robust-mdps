#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "algorithms/mdp.hpp"
#include "algorithms/fast/bellman/bellman.hpp"
#include "algorithms/fast/kl/projection/project.hpp"
#include "algorithms/fast/burg/projection/project.hpp"
#include "algorithms/homotopy/bellman/bellman.hpp"
#include "algorithms/cplex/l1/bellman/bellman.hpp"
#include "algorithms/cplex/l2/bellman/bellman.hpp"
#include "algorithms/gurobi/l1/bellman/bellman.hpp"
#include "algorithms/gurobi/l2/bellman/bellman.hpp"
#include "algorithms/mosek/l1/bellman/bellman.hpp"
#include "algorithms/mosek/l2/bellman/bellman.hpp"
#include "algorithms/mosek/kl/bellman/bellman.hpp"
#include "algorithms/mosek/burg/bellman/bellman.hpp"

namespace {
struct RunStats {
  double sum_abs = 0.0;
  double max_abs = 0.0;
  int count = 0;
  int used_states = 0;
  int failed_states = 0;
};
}

int main() {
  std::cout.setf(std::ios::unitbuf);
  std::mt19937 rng(0);

  std::vector<std::string> roots = {"instances/synthetic", "instances/benchmarks"};
  if (!std::filesystem::exists("instances")) {
    roots = {"../../../instances/synthetic", "../../../instances/benchmarks"};
  }
  std::vector<std::filesystem::path> instance_dirs;
  for (const auto& root : roots) {
    if (!std::filesystem::exists(root)) {
      continue;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
      if (!entry.is_directory()) {
        continue;
      }
      auto mdp_path = entry.path() / "nominal.mdp";
      if (std::filesystem::exists(mdp_path)) {
        instance_dirs.push_back(entry.path());
      }
    }
  }

  if (instance_dirs.empty()) {
    std::cout << "No instances found.\n";
    return 0;
  }

  for (const auto& dir : instance_dirs) {
    std::vector<std::filesystem::path> amb_files;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".amb") {
        amb_files.push_back(entry.path());
      }
    }
    if (amb_files.empty()) {
      std::cout << dir.string() << ": no ambiguity files found\n";
      continue;
    }

    for (const auto& amb_path : amb_files) {
      std::cout << "Processing " << dir.string() << " [" << amb_path.filename().string() << "]\n";
      auto mdp_path = dir / "nominal.mdp";
      mdp::AmbiguousMDP data = mdp::load_mdp_amb(mdp_path.string(), amb_path.string());
      const int S = data.mdp.n_states;
      const int A = data.mdp.n_actions;

      std::vector<double> transitions_orig = data.mdp.transitions;
      std::vector<double> transitions_bumped = data.mdp.transitions;
      double bump = 1.0 / (10.0 * static_cast<double>(S));
      for (int s = 0; s < S; ++s) {
        for (int a = 0; a < A; ++a) {
          double sum = 0.0;
          for (int sp = 0; sp < S; ++sp) {
            int idx = mdp::index(s, a, sp, S, A);
            transitions_bumped[idx] += bump;
            sum += transitions_bumped[idx];
          }
          for (int sp = 0; sp < S; ++sp) {
            int idx = mdp::index(s, a, sp, S, A);
            transitions_bumped[idx] /= sum;
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
      std::uniform_real_distribution<double> dist(0.0, vmax);
      std::vector<double> V(S, 0.0);
      for (int i = 0; i < S; ++i) {
        V[i] = dist(rng);
      }

      for (const std::string& divergence : {"L1", "L2", "KL", "BURG"}) {
        std::cout << "  " << divergence << ": starting\n";
        RunStats stats;
        RunStats stats_homotopy_cplex;
        RunStats stats_homotopy_gurobi;
        RunStats stats_homotopy_mosek;

        for (int s = 0; s < S; ++s) {
          std::vector<double> rewards_sa(A * S, 0.0);
          std::vector<double> transitions_sa(A * S, 0.0);
          std::vector<double> sigma_sa(A * S, 1.0);

          for (int a = 0; a < A; ++a) {
            for (int sp = 0; sp < S; ++sp) {
              int idx_global = mdp::index(s, a, sp, S, A);
              int idx_local = a * S + sp;
              rewards_sa[idx_local] = data.mdp.rewards[idx_global];
              if (divergence == "KL" || divergence == "BURG") {
                transitions_sa[idx_local] = transitions_bumped[idx_global];
              } else {
                transitions_sa[idx_local] = transitions_orig[idx_global];
              }
              if (divergence == "L1") {
                sigma_sa[idx_local] = data.amb.sigma_l1[idx_global];
              } else if (divergence == "L2") {
                sigma_sa[idx_local] = data.amb.sigma_l2[idx_global];
              }
            }
          }

          std::vector<std::pair<std::string, double>> results;
          try {
            if (divergence == "L1") {
              homotopy_l1_bellman::BellmanInput in_homotopy;
              in_homotopy.n_states = S;
              in_homotopy.n_actions = A;
              in_homotopy.discount = data.mdp.discount;
              in_homotopy.kappa = data.amb.kappa_l1;
              in_homotopy.v = V;
              in_homotopy.rewards = rewards_sa;
              in_homotopy.transitions = transitions_sa;
              in_homotopy.sigma = sigma_sa;

              cplex_l1_bellman::BellmanInput in_cplex;
              in_cplex.n_states = S;
              in_cplex.n_actions = A;
              in_cplex.discount = data.mdp.discount;
              in_cplex.kappa = data.amb.kappa_l1;
              in_cplex.v = V;
              in_cplex.rewards = rewards_sa;
              in_cplex.transitions = transitions_sa;
              in_cplex.sigma = sigma_sa;

              mosek_l1_bellman::BellmanInput in_mosek;
              in_mosek.n_states = S;
              in_mosek.n_actions = A;
              in_mosek.discount = data.mdp.discount;
              in_mosek.kappa = data.amb.kappa_l1;
              in_mosek.v = V;
              in_mosek.rewards = rewards_sa;
              in_mosek.transitions = transitions_sa;
              in_mosek.sigma = sigma_sa;

              double homotopy_value = homotopy_l1_bellman::solve_bellman_state(in_homotopy);
              double cplex_value = cplex_l1_bellman::solve_bellman_state(in_cplex);
              gurobi_l1_bellman::BellmanInput in_gurobi;
              in_gurobi.n_states = S;
              in_gurobi.n_actions = A;
              in_gurobi.discount = data.mdp.discount;
              in_gurobi.kappa = data.amb.kappa_l1;
              in_gurobi.v = V;
              in_gurobi.rewards = rewards_sa;
              in_gurobi.transitions = transitions_sa;
              in_gurobi.sigma = sigma_sa;
              double gurobi_value = gurobi_l1_bellman::solve_bellman_state(in_gurobi);
              double mosek_value = mosek_l1_bellman::solve_bellman_state(in_mosek);

              results.emplace_back("cplex_l1", cplex_value);
              results.emplace_back("gurobi_l1", gurobi_value);
              results.emplace_back("mosek_l1", mosek_value);

              double diff_cplex = std::abs(homotopy_value - cplex_value);
              stats_homotopy_cplex.sum_abs += diff_cplex;
              stats_homotopy_cplex.max_abs =
                  std::max(stats_homotopy_cplex.max_abs, diff_cplex);
              stats_homotopy_cplex.count += 1;
              stats_homotopy_cplex.used_states += 1;

              double diff_gurobi = std::abs(homotopy_value - gurobi_value);
              stats_homotopy_gurobi.sum_abs += diff_gurobi;
              stats_homotopy_gurobi.max_abs =
                  std::max(stats_homotopy_gurobi.max_abs, diff_gurobi);
              stats_homotopy_gurobi.count += 1;
              stats_homotopy_gurobi.used_states += 1;

              double diff_mosek = std::abs(homotopy_value - mosek_value);
              stats_homotopy_mosek.sum_abs += diff_mosek;
              stats_homotopy_mosek.max_abs =
                  std::max(stats_homotopy_mosek.max_abs, diff_mosek);
              stats_homotopy_mosek.count += 1;
              stats_homotopy_mosek.used_states += 1;
            } else if (divergence == "L2") {
              cplex_l2_bellman::BellmanInput in_cplex;
              in_cplex.n_states = S;
              in_cplex.n_actions = A;
              in_cplex.discount = data.mdp.discount;
              in_cplex.kappa = data.amb.kappa_l2;
              in_cplex.v = V;
              in_cplex.rewards = rewards_sa;
              in_cplex.transitions = transitions_sa;
              in_cplex.sigma = sigma_sa;

              mosek_l2_bellman::BellmanInput in_mosek;
              in_mosek.n_states = S;
              in_mosek.n_actions = A;
              in_mosek.discount = data.mdp.discount;
              in_mosek.kappa = data.amb.kappa_l2;
              in_mosek.v = V;
              in_mosek.rewards = rewards_sa;
              in_mosek.transitions = transitions_sa;
              in_mosek.sigma = sigma_sa;

              results.emplace_back("cplex_l2", cplex_l2_bellman::solve_bellman_state(in_cplex));
              gurobi_l2_bellman::BellmanInput in_gurobi;
              in_gurobi.n_states = S;
              in_gurobi.n_actions = A;
              in_gurobi.discount = data.mdp.discount;
              in_gurobi.kappa = data.amb.kappa_l2;
              in_gurobi.v = V;
              in_gurobi.rewards = rewards_sa;
              in_gurobi.transitions = transitions_sa;
              in_gurobi.sigma = sigma_sa;
              results.emplace_back("gurobi_l2", gurobi_l2_bellman::solve_bellman_state(in_gurobi));
              results.emplace_back("mosek_l2", mosek_l2_bellman::solve_bellman_state(in_mosek));
            } else if (divergence == "KL") {
              fast_bellman::BellmanStateInput fast_in;
              fast_in.n_states = S;
              fast_in.n_actions = A;
              fast_in.discount = data.mdp.discount;
              fast_in.kappa = data.amb.kappa_kl;
              fast_in.epsilon = 1e-6;
              fast_in.lower_bound = -1.0;
              fast_in.upper_bound = -1.0;
              fast_in.v = V;
              fast_in.rewards = rewards_sa;
              fast_in.transitions = transitions_sa;

              auto fast_proj = [](const fast_bellman::ProjectionInput& in) {
                fast_kl::ProjectionInput proj{*in.pbar, *in.b, in.beta};
                return fast_kl::solve_projection_problem(proj).objective;
              };

              mosek_kl_bellman::BellmanInput in_mosek;
              in_mosek.n_states = S;
              in_mosek.n_actions = A;
              in_mosek.discount = data.mdp.discount;
              in_mosek.kappa = data.amb.kappa_kl;
              in_mosek.v = V;
              in_mosek.rewards = rewards_sa;
              in_mosek.transitions = transitions_sa;

              results.emplace_back("fast_kl", fast_bellman::solve_robust_bellman_state(fast_in, fast_proj));
              results.emplace_back("mosek_kl", mosek_kl_bellman::solve_bellman_state(in_mosek));
            } else {
              fast_bellman::BellmanStateInput fast_in;
              fast_in.n_states = S;
              fast_in.n_actions = A;
              fast_in.discount = data.mdp.discount;
              fast_in.kappa = data.amb.kappa_burg;
              fast_in.epsilon = 1e-6;
              fast_in.lower_bound = -1.0;
              fast_in.upper_bound = -1.0;
              fast_in.v = V;
              fast_in.rewards = rewards_sa;
              fast_in.transitions = transitions_sa;

              auto fast_proj = [](const fast_bellman::ProjectionInput& in) {
                fast_burg::ProjectionInput proj{*in.pbar, *in.b, in.beta};
                return fast_burg::solve_projection_problem(proj).objective;
              };

              mosek_burg_bellman::BellmanInput in_mosek;
              in_mosek.n_states = S;
              in_mosek.n_actions = A;
              in_mosek.discount = data.mdp.discount;
              in_mosek.kappa = data.amb.kappa_burg;
              in_mosek.v = V;
              in_mosek.rewards = rewards_sa;
              in_mosek.transitions = transitions_sa;

              results.emplace_back("fast_burg", fast_bellman::solve_robust_bellman_state(fast_in, fast_proj));
              results.emplace_back("mosek_burg", mosek_burg_bellman::solve_bellman_state(in_mosek));
            }
          } catch (const std::exception& exc) {
            std::cout << "      exception at s=" << s << " (" << divergence
                      << "): " << exc.what() << "\n";
            stats.failed_states += 1;
            if (divergence == "L1") {
              stats_homotopy_cplex.failed_states += 1;
              stats_homotopy_gurobi.failed_states += 1;
              stats_homotopy_mosek.failed_states += 1;
            }
            continue;
          }

          if (results.size() >= 2) {
            double diff = std::abs(results[0].second - results[1].second);
            stats.sum_abs += diff;
            stats.max_abs = std::max(stats.max_abs, diff);
            stats.count += 1;
            stats.used_states += 1;
          }
        }

        if (stats.count == 0) {
          std::cout << dir.string() << " [" << amb_path.filename().string() << "] "
                    << divergence << ": no admissible runs\n";
          continue;
        }

        std::cout << dir.string() << " [" << amb_path.filename().string() << "] "
                  << divergence << ":\n";
        std::cout << "  avg abs: " << (stats.sum_abs / stats.count)
                  << ", max abs: " << stats.max_abs << "\n";
        std::cout << "  states used: " << stats.used_states
                  << ", states failed: " << stats.failed_states << "\n";
        if (divergence == "L1") {
          if (stats_homotopy_cplex.count > 0) {
            std::cout << "  homotopy vs cplex avg abs: "
                      << (stats_homotopy_cplex.sum_abs / stats_homotopy_cplex.count)
                      << ", max abs: " << stats_homotopy_cplex.max_abs << "\n";
          }
          if (stats_homotopy_gurobi.count > 0) {
            std::cout << "  homotopy vs gurobi avg abs: "
                      << (stats_homotopy_gurobi.sum_abs / stats_homotopy_gurobi.count)
                      << ", max abs: " << stats_homotopy_gurobi.max_abs << "\n";
          }
          if (stats_homotopy_mosek.count > 0) {
            std::cout << "  homotopy vs mosek avg abs: "
                      << (stats_homotopy_mosek.sum_abs / stats_homotopy_mosek.count)
                      << ", max abs: " << stats_homotopy_mosek.max_abs << "\n";
          }
        }
      }
    }
  }

  return 0;
}
