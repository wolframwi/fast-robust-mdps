#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "algorithms/mdp.hpp"
#include "algorithms/projection_utils.hpp"

#include "algorithms/cplex/l1/projection/project.hpp"
#include "algorithms/cplex/l2/projection/project.hpp"
#include "algorithms/gurobi/l1/projection/project.hpp"
#include "algorithms/gurobi/l2/projection/project.hpp"
#include "algorithms/mosek/l1/projection/project.hpp"
#include "algorithms/mosek/l2/projection/project.hpp"
#include "algorithms/mosek/kl/projection/project.hpp"
#include "algorithms/mosek/burg/projection/project.hpp"
#include "algorithms/fast/l1/projection/project.hpp"
#include "algorithms/fast/kl/projection/project.hpp"
#include "algorithms/fast/burg/projection/project.hpp"

namespace {
struct RunStats {
  double sum_linf = 0.0;
  double sum_l1 = 0.0;
  double sum_cons = 0.0;
  double sum_obj = 0.0;
  double max_linf = 0.0;
  double max_l1 = 0.0;
  double max_cons = 0.0;
  double max_obj = 0.0;
  int count = 0;
  int used_rows = 0;
  int failed_rows = 0;
};

std::vector<double> adjust_pbar_positive(const std::vector<double>& pbar,
                                         double eps = 1e-12) {
  bool all_positive = true;
  for (double v : pbar) {
    if (v <= 0.0) {
      all_positive = false;
      break;
    }
  }
  if (all_positive) {
    return pbar;
  }
  std::vector<double> adjusted(pbar.size(), 0.0);
  double total = 0.0;
  for (size_t i = 0; i < pbar.size(); ++i) {
    adjusted[i] = std::max(eps, pbar[i]);
    total += adjusted[i];
  }
  for (double& v : adjusted) {
    v /= total;
  }
  return adjusted;
}

double dot(const std::vector<double>& a, const std::vector<double>& b) {
  double total = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    total += a[i] * b[i];
  }
  return total;
}

double max_abs_diff(const std::vector<double>& a, const std::vector<double>& b) {
  double m = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    m = std::max(m, std::abs(a[i] - b[i]));
  }
  return m;
}

double l1_diff(const std::vector<double>& a, const std::vector<double>& b) {
  double total = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    total += std::abs(a[i] - b[i]);
  }
  return total;
}

} // namespace

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

      for (const std::string& divergence : {"L1", "L2", "KL", "BURG"}) {
        std::cout << "  " << divergence << ": starting\n";
        RunStats stats;
        std::vector<double> V(S, 0.0);
        for (int i = 0; i < S; ++i) {
          V[i] = dist(rng);
        }
        double min_b = *std::min_element(V.begin(), V.end());

        double run_linf = 0.0;
        double run_l1 = 0.0;
        double run_cons = 0.0;
        double run_obj = 0.0;
        int run_rows = 0;
        int run_failed = 0;

        for (int s = 0; s < S; ++s) {
          for (int a = 0; a < A; ++a) {
            std::vector<double> pbar(S, 0.0);
            std::vector<double> weights(S, 1.0);
            for (int sp = 0; sp < S; ++sp) {
              int idx = mdp::index(s, a, sp, S, A);
              if (divergence == "KL" || divergence == "BURG") {
                pbar[sp] = transitions_bumped[idx];
              } else {
                pbar[sp] = transitions_orig[idx];
              }
              if (divergence == "L1") {
                weights[sp] = data.amb.sigma_l1[idx];
              } else if (divergence == "L2") {
                weights[sp] = data.amb.sigma_l2[idx];
              }
            }

            std::vector<double> pbar_ref = pbar;
            if (divergence == "KL" || divergence == "BURG") {
              pbar_ref = adjust_pbar_positive(pbar_ref);
            }

            double baseline = dot(pbar_ref, V);
            double beta = baseline;
            if (baseline > min_b) {
              beta = 0.5 * (baseline + min_b);
            }

            struct ResultEntry {
              std::string name;
              std::vector<double> p;
              double objective = 0.0;
              bool has_objective = false;
            };
            std::vector<ResultEntry> results;
            try {
              if (divergence == "L1") {
                cplex_l1::ProjectionInput in_cplex{pbar_ref, V, weights, beta};
                gurobi_l1::ProjectionInput in_gurobi{pbar_ref, V, weights, beta};
                mosek_l1::ProjectionInput in_mosek{pbar_ref, V, weights, beta};
                fast_l1::ProjectionInput in_fast{pbar_ref, V, weights, beta};
                results.push_back({"cplex_l1", cplex_l1::solve_projection_problem(in_cplex).p});
                results.push_back({"gurobi_l1", gurobi_l1::solve_projection_problem(in_gurobi).p});
                results.push_back({"mosek_l1", mosek_l1::solve_projection_problem(in_mosek).p});
                auto fast_bisect = fast_l1::solve_projection_problem(in_fast);
                results.push_back({"fast_l1", fast_bisect.p, fast_bisect.objective, true});
              } else if (divergence == "L2") {
                cplex_l2::ProjectionInput in_cplex{pbar_ref, V, weights, beta};
                gurobi_l2::ProjectionInput in_gurobi{pbar_ref, V, weights, beta};
                mosek_l2::ProjectionInput in_mosek{pbar_ref, V, weights, beta};
                results.push_back({"cplex_l2", cplex_l2::solve_projection_problem(in_cplex).p});
                results.push_back({"gurobi_l2", gurobi_l2::solve_projection_problem(in_gurobi).p});
                results.push_back({"mosek_l2", mosek_l2::solve_projection_problem(in_mosek).p});
              } else if (divergence == "KL") {
                mosek_kl::ProjectionInput in_mosek{pbar_ref, V, beta};
                fast_kl::ProjectionInput in_fast{pbar_ref, V, beta};
                results.push_back({"mosek_kl", mosek_kl::solve_projection_problem(in_mosek).p});
                results.push_back({"fast_kl", fast_kl::solve_projection_problem(in_fast).p});
              } else {
                mosek_burg::ProjectionInput in_mosek{pbar_ref, V, beta};
                fast_burg::ProjectionInput in_fast{pbar_ref, V, beta};
                results.push_back({"mosek_burg", mosek_burg::solve_projection_problem(in_mosek).p});
                results.push_back({"fast_burg", fast_burg::solve_projection_problem(in_fast).p});
              }
            } catch (const std::exception& exc) {
              std::cout << "      exception at s=" << s << ", a=" << a
                        << " (" << divergence << "): " << exc.what() << "\n";
              run_failed += 1;
              continue;
            }

            if (results.size() < 2) {
              continue;
            }

            double row_linf = 0.0;
            double row_l1 = 0.0;
            double row_cons = 0.0;
            double row_obj = 0.0;
            for (size_t i = 0; i < results.size(); ++i) {
              for (size_t j = i + 1; j < results.size(); ++j) {
                const auto& pA = results[i].p;
                const auto& pB = results[j].p;
                double objA = 0.0;
                double objB = 0.0;
                if (!pA.empty() && !pB.empty()) {
                  row_linf = std::max(row_linf, max_abs_diff(pA, pB));
                  row_l1 = std::max(row_l1, l1_diff(pA, pB));
                  row_cons = std::max(row_cons, std::abs(dot(V, pA) - dot(V, pB)));
                }
                if (results[i].has_objective) {
                  objA = results[i].objective;
                } else if (!pA.empty()) {
                  if (divergence == "L1") {
                    objA = projection_utils::l1_distance(pA, pbar_ref, weights);
                  } else if (divergence == "L2") {
                    objA = projection_utils::l2_distance(pA, pbar_ref, weights);
                  } else if (divergence == "KL") {
                    objA = projection_utils::kl_divergence(pA, pbar_ref);
                  } else {
                    objA = projection_utils::burg_divergence(pA, pbar_ref);
                  }
                }
                if (results[j].has_objective) {
                  objB = results[j].objective;
                } else if (!pB.empty()) {
                  if (divergence == "L1") {
                    objB = projection_utils::l1_distance(pB, pbar_ref, weights);
                  } else if (divergence == "L2") {
                    objB = projection_utils::l2_distance(pB, pbar_ref, weights);
                  } else if (divergence == "KL") {
                    objB = projection_utils::kl_divergence(pB, pbar_ref);
                  } else {
                    objB = projection_utils::burg_divergence(pB, pbar_ref);
                  }
                }
                row_obj = std::max(row_obj, std::abs(objA - objB));
              }
            }

            run_linf = std::max(run_linf, row_linf);
            run_l1 = std::max(run_l1, row_l1);
            run_cons = std::max(run_cons, row_cons);
            run_obj = std::max(run_obj, row_obj);
            run_rows += 1;
          }
        }

        if (run_rows > 0) {
          stats.sum_linf += run_linf;
          stats.sum_l1 += run_l1;
          stats.sum_cons += run_cons;
          stats.sum_obj += run_obj;
          stats.max_linf = std::max(stats.max_linf, run_linf);
          stats.max_l1 = std::max(stats.max_l1, run_l1);
          stats.max_cons = std::max(stats.max_cons, run_cons);
          stats.max_obj = std::max(stats.max_obj, run_obj);
          stats.count += 1;
          stats.used_rows += run_rows;
          stats.failed_rows += run_failed;
        }

        if (stats.count == 0) {
          std::cout << dir.string() << " [" << amb_path.filename().string() << "] "
                    << divergence << ": no admissible runs\n";
          continue;
        }

        std::cout << dir.string() << " [" << amb_path.filename().string() << "] "
                  << divergence << ":\n";
        std::cout << "  avg linf: " << (stats.sum_linf / stats.count)
                  << ", max linf: " << stats.max_linf << "\n";
        std::cout << "  avg l1:   " << (stats.sum_l1 / stats.count)
                  << ", max l1:   " << stats.max_l1 << "\n";
        std::cout << "  avg cons: " << (stats.sum_cons / stats.count)
                  << ", max cons: " << stats.max_cons << "\n";
        std::cout << "  avg obj:  " << (stats.sum_obj / stats.count)
                  << ", max obj:  " << stats.max_obj << "\n";
        std::cout << "  rows used: " << stats.used_rows
                  << ", rows failed: " << stats.failed_rows << "\n";
      }
    }
  }

  return 0;
}
