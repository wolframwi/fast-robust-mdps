#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "algorithms/mdp.hpp"
#include "algorithms/fast/bellman/bellman.hpp"
#include "algorithms/fast/l2/projection/project.hpp"
#include "algorithms/cplex/l2/bellman/bellman.hpp"
#include "algorithms/gurobi/l2/bellman/bellman.hpp"
#include "algorithms/mosek/l2/bellman/bellman.hpp"
#include "algorithms/value_iteration.hpp"

namespace {

std::string sanitize_name(const std::string& path) {
  std::string out;
  out.reserve(path.size());
  for (char c : path) {
    if (c == '/' || c == '\\' || c == ':' || c == ' ') {
      out.push_back('_');
    } else if (c == '.') {
      continue;
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::vector<std::filesystem::path> collect_instances(const std::vector<std::string>& roots) {
  std::vector<std::filesystem::path> dirs;
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
        dirs.push_back(entry.path());
      }
    }
  }
  return dirs;
}

} // namespace

int main() {
  std::cout.setf(std::ios::unitbuf);
  std::vector<std::string> roots = {
      "instances/benchmarks",
      "../../../../instances/benchmarks",
  };

  std::vector<std::filesystem::path> instance_dirs = collect_instances(roots);
  if (instance_dirs.empty()) {
    std::cout << "No instances found.\n";
    return 0;
  }

  std::filesystem::path runtime_dir = "runtime";
  std::filesystem::path error_dir = "errors";
  std::filesystem::create_directories(runtime_dir);
  std::filesystem::create_directories(error_dir);

  int total_instances = 0;
  int fast_failed_instances = 0;
  int cplex_failed_instances = 0;
  int gurobi_failed_instances = 0;
  int mosek_failed_instances = 0;

  for (const auto& dir : instance_dirs) {
    std::vector<std::filesystem::path> amb_files;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".amb") {
        amb_files.push_back(entry.path());
      }
    }
    if (amb_files.empty()) {
      continue;
    }

    for (const auto& amb_path : amb_files) {
      std::string base_name = sanitize_name(dir.string());
      std::string amb_name = amb_path.stem().string();
      std::filesystem::path runtime_path = runtime_dir / (base_name + "__" + amb_name + ".txt");
      std::filesystem::path error_path = error_dir / (base_name + "__" + amb_name + ".log");
      if (std::filesystem::exists(runtime_path)) {
        std::cout << "Skipping existing " << runtime_path.string() << "\n";
        continue;
      }

      total_instances += 1;
      std::cout << "Processing " << dir.string() << " ["
                << amb_path.filename().string() << "]\n";
      std::cout << "  L2: value iteration\n";

      auto mdp_path = dir / "nominal.mdp";
      mdp::AmbiguousMDP data = mdp::load_mdp_amb(mdp_path.string(), amb_path.string());
      const int S = data.mdp.n_states;
      const int A = data.mdp.n_actions;

      std::vector<std::vector<double>> rewards_by_state(S, std::vector<double>(A * S, 0.0));
      std::vector<std::vector<double>> transitions_by_state(S, std::vector<double>(A * S, 0.0));
      std::vector<std::vector<double>> sigma_by_state(S, std::vector<double>(A * S, 0.0));
      for (int s = 0; s < S; ++s) {
        for (int a = 0; a < A; ++a) {
          for (int sp = 0; sp < S; ++sp) {
            int idx_global = mdp::index(s, a, sp, S, A);
            int idx_local = a * S + sp;
            rewards_by_state[s][idx_local] = data.mdp.rewards[idx_global];
            transitions_by_state[s][idx_local] = data.mdp.transitions[idx_global];
            sigma_by_state[s][idx_local] = data.amb.sigma_l2[idx_global];
          }
        }
      }

      std::ofstream runtime_out(runtime_path);
      std::ofstream error_out(error_path);
      error_out << "instance=" << dir.string() << "\n";
      error_out << "amb=" << amb_path.filename().string() << "\n";

      auto fast_proj = [](const fast_bellman::ProjectionInput& in) {
        fast_l2::ProjectionInput proj{*in.pbar, *in.b, *in.weights, in.beta};
        return fast_l2::solve_projection_problem(proj).objective;
      };

      int fast_iterations = -1;
      int cplex_iterations = -1;
      int gurobi_iterations = -1;
      int mosek_iterations = -1;
      double inf_diff_cplex = -1.0;
      double inf_diff_gurobi = -1.0;
      double inf_diff_mosek = -1.0;
      double fast_time_us = 0.0;
      double cplex_time_us = 0.0;
      double gurobi_time_us = 0.0;
      double mosek_time_us = 0.0;
      bool fast_ok = false;
      bool cplex_ok = false;
      bool gurobi_ok = false;
      bool mosek_ok = false;

      value_iteration::Result fast_result;
      try {
        std::cout << "    starting fast value iteration\n";
        auto fast_op = [&](int s, const std::vector<double>& v) {
          fast_bellman::BellmanStateInput in;
          in.n_states = S;
          in.n_actions = A;
          in.discount = data.mdp.discount;
          in.kappa = data.amb.kappa_l2;
          in.epsilon = 1e-6;
          in.lower_bound = -1.0;
          in.upper_bound = -1.0;
          in.v = v;
          in.rewards = rewards_by_state[s];
          in.transitions = transitions_by_state[s];
          in.sigma = sigma_by_state[s];
          return fast_bellman::solve_robust_bellman_state(in, fast_proj);
        };
        auto t0 = std::chrono::steady_clock::now();
        fast_result = value_iteration::solve(S, fast_op, 1e-4, &std::cout, 25, 2500);
        auto t1 = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::micro> dt = t1 - t0;
        fast_time_us = dt.count();
        fast_iterations = fast_result.iterations;
        fast_ok = true;
      } catch (const std::exception& exc) {
        error_out << "exception value_iteration fast " << exc.what() << "\n";
        fast_failed_instances += 1;
      }

      value_iteration::Result cplex_result;
      try {
        std::cout << "    starting CPLEX value iteration\n";
        auto cplex_op = [&](int s, const std::vector<double>& v) {
          cplex_l2_bellman::BellmanInput in;
          in.n_states = S;
          in.n_actions = A;
          in.discount = data.mdp.discount;
          in.kappa = data.amb.kappa_l2;
          in.v = v;
          in.rewards = rewards_by_state[s];
          in.transitions = transitions_by_state[s];
          in.sigma = sigma_by_state[s];
          return cplex_l2_bellman::solve_bellman_state(in);
        };
        auto t0 = std::chrono::steady_clock::now();
        cplex_result = value_iteration::solve(S, cplex_op, 1e-4, &std::cout, 25, 2500);
        auto t1 = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::micro> dt = t1 - t0;
        cplex_time_us = dt.count();
        cplex_iterations = cplex_result.iterations;
        cplex_ok = true;
      } catch (const std::exception& exc) {
        error_out << "exception value_iteration cplex " << exc.what() << "\n";
        cplex_failed_instances += 1;
      }

      value_iteration::Result mosek_result;
      try {
        std::cout << "    starting MOSEK value iteration\n";
        auto mosek_op = [&](int s, const std::vector<double>& v) {
          mosek_l2_bellman::BellmanInput in;
          in.n_states = S;
          in.n_actions = A;
          in.discount = data.mdp.discount;
          in.kappa = data.amb.kappa_l2;
          in.v = v;
          in.rewards = rewards_by_state[s];
          in.transitions = transitions_by_state[s];
          in.sigma = sigma_by_state[s];
          return mosek_l2_bellman::solve_bellman_state(in);
        };
        auto t0 = std::chrono::steady_clock::now();
        mosek_result = value_iteration::solve(S, mosek_op, 1e-4, &std::cout, 25, 2500);
        auto t1 = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::micro> dt = t1 - t0;
        mosek_time_us = dt.count();
        mosek_iterations = mosek_result.iterations;
        mosek_ok = true;
      } catch (const std::exception& exc) {
        error_out << "exception value_iteration mosek " << exc.what() << "\n";
        mosek_failed_instances += 1;
      }

      value_iteration::Result gurobi_result;
      try {
        std::cout << "    starting Gurobi value iteration\n";
        auto gurobi_op = [&](int s, const std::vector<double>& v) {
          gurobi_l2_bellman::BellmanInput in;
          in.n_states = S;
          in.n_actions = A;
          in.discount = data.mdp.discount;
          in.kappa = data.amb.kappa_l2;
          in.v = v;
          in.rewards = rewards_by_state[s];
          in.transitions = transitions_by_state[s];
          in.sigma = sigma_by_state[s];
          return gurobi_l2_bellman::solve_bellman_state(in);
        };
        auto t0 = std::chrono::steady_clock::now();
        gurobi_result = value_iteration::solve(S, gurobi_op, 1e-4, &std::cout, 25, 2500);
        auto t1 = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::micro> dt = t1 - t0;
        gurobi_time_us = dt.count();
        gurobi_iterations = gurobi_result.iterations;
        gurobi_ok = true;
      } catch (const std::exception& exc) {
        error_out << "exception value_iteration gurobi " << exc.what() << "\n";
        gurobi_failed_instances += 1;
      }

      if (fast_ok && cplex_ok) {
        inf_diff_cplex = 0.0;
        for (int i = 0; i < S; ++i) {
          inf_diff_cplex =
              std::max(inf_diff_cplex, std::abs(fast_result.value[i] - cplex_result.value[i]));
        }
      }
      if (fast_ok && gurobi_ok) {
        inf_diff_gurobi = 0.0;
        for (int i = 0; i < S; ++i) {
          inf_diff_gurobi =
              std::max(inf_diff_gurobi, std::abs(fast_result.value[i] - gurobi_result.value[i]));
        }
      }
      if (fast_ok && mosek_ok) {
        inf_diff_mosek = 0.0;
        for (int i = 0; i < S; ++i) {
          inf_diff_mosek =
              std::max(inf_diff_mosek, std::abs(fast_result.value[i] - mosek_result.value[i]));
        }
      }

      runtime_out << "instance=" << dir.string() << "\n";
      runtime_out << "amb=" << amb_path.filename().string() << "\n";
      runtime_out << "fast_vi_iterations=" << fast_iterations << "\n";
      runtime_out << "cplex_vi_iterations=" << cplex_iterations << "\n";
      runtime_out << "gurobi_vi_iterations=" << gurobi_iterations << "\n";
      runtime_out << "mosek_vi_iterations=" << mosek_iterations << "\n";
      runtime_out << "fast_vi_s=" << (fast_time_us * 1e-6) << "\n";
      runtime_out << "cplex_vi_s=" << (cplex_time_us * 1e-6) << "\n";
      runtime_out << "gurobi_vi_s=" << (gurobi_time_us * 1e-6) << "\n";
      runtime_out << "mosek_vi_s=" << (mosek_time_us * 1e-6) << "\n";
      runtime_out << "vi_infty_diff_fast_cplex=" << inf_diff_cplex << "\n";
      runtime_out << "vi_infty_diff_fast_gurobi=" << inf_diff_gurobi << "\n";
      runtime_out << "vi_infty_diff_fast_mosek=" << inf_diff_mosek << "\n";
      runtime_out << "fast_failed=" << (fast_ok ? 0 : 1) << "\n";
      runtime_out << "cplex_failed=" << (cplex_ok ? 0 : 1) << "\n";
      runtime_out << "gurobi_failed=" << (gurobi_ok ? 0 : 1) << "\n";
      runtime_out << "mosek_failed=" << (mosek_ok ? 0 : 1) << "\n";
    }
  }

  std::filesystem::path summary_path = runtime_dir / "summary.txt";
  std::ofstream summary_out(summary_path);
  summary_out << "total_instances=" << total_instances << "\n";
  summary_out << "fast_failed_instances=" << fast_failed_instances << "\n";
  summary_out << "cplex_failed_instances=" << cplex_failed_instances << "\n";
  summary_out << "gurobi_failed_instances=" << gurobi_failed_instances << "\n";
  summary_out << "mosek_failed_instances=" << mosek_failed_instances << "\n";

  std::cout << "Summary: total_instances=" << total_instances
            << " fast_failed_instances=" << fast_failed_instances
            << " cplex_failed_instances=" << cplex_failed_instances
            << " gurobi_failed_instances=" << gurobi_failed_instances
            << " mosek_failed_instances=" << mosek_failed_instances << "\n";
  return 0;
}
