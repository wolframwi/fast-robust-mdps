#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <ilcplex/ilocplex.h>
#include "gurobi_c++.h"

#include "algorithms/mdp.hpp"
#include "algorithms/fast/bellman/bellman.hpp"
#include "algorithms/fast/l2/projection/project.hpp"
#include "algorithms/cplex/l2/bellman/bellman.hpp"
#include "algorithms/gurobi/l2/bellman/bellman.hpp"
#include "algorithms/mosek/l2/bellman/bellman.hpp"

namespace {

bool matches_instance(const std::filesystem::path& dir, const std::string& filter) {
  if (filter.empty()) {
    return true;
  }
  std::filesystem::path filter_path(filter);
  if (dir == filter_path || dir.string() == filter) {
    return true;
  }
  if (filter_path.is_relative() && dir.filename() == filter_path) {
    return true;
  }
  try {
    if (std::filesystem::exists(filter_path) && std::filesystem::equivalent(dir, filter_path)) {
      return true;
    }
  } catch (...) {
  }
  return false;
}

bool matches_amb(const std::filesystem::path& amb_path, const std::string& filter) {
  if (filter.empty()) {
    return true;
  }
  std::filesystem::path filter_path(filter);
  if (amb_path == filter_path || amb_path.string() == filter) {
    return true;
  }
  if (amb_path.filename() == filter_path || amb_path.filename().string() == filter) {
    return true;
  }
  if (amb_path.stem() == filter_path || amb_path.stem().string() == filter) {
    return true;
  }
  return false;
}

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

double quantile(std::vector<double> values, double q) {
  if (values.empty()) {
    return 0.0;
  }
  double pos = q * (values.size() - 1);
  size_t idx = static_cast<size_t>(pos);
  std::nth_element(values.begin(), values.begin() + idx, values.end());
  return values[idx];
}

} // namespace

int main(int argc, char** argv) {
  std::string filter_instance;
  std::string filter_amb;
  std::string solver = "all";
  bool a10_only = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--instance" && i + 1 < argc) {
      filter_instance = argv[++i];
    } else if (arg == "--amb" && i + 1 < argc) {
      filter_amb = argv[++i];
    } else if (arg == "--solver" && i + 1 < argc) {
      solver = argv[++i];
    } else if (arg == "--a10-only") {
      a10_only = true;
    } else if (arg == "--help") {
      std::cout
          << "Usage: run_l2_bellman_runtime [--instance PATH] [--amb PATH|NAME] "
             "[--solver fast|cplex|gurobi|mosek|all] [--a10-only]\n";
      return 0;
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      return 1;
    }
  }

  const bool run_all = (solver == "all");
  const bool run_fast = run_all || solver == "fast";
  const bool run_cplex = run_all || solver == "cplex";
  const bool run_gurobi = run_all || solver == "gurobi";
  const bool run_mosek = run_all || solver == "mosek";
  if (!(run_fast || run_cplex || run_gurobi || run_mosek)) {
    std::cerr << "Unsupported solver: " << solver << "\n";
    return 1;
  }

  std::unique_ptr<IloEnv> cplex_env;
  if (run_cplex) {
    cplex_env = std::make_unique<IloEnv>();
  }
  std::unique_ptr<GRBEnv> gurobi_env;
  if (run_gurobi) {
    gurobi_env = std::make_unique<GRBEnv>(true);
    gurobi_env->set(GRB_IntParam_OutputFlag, 0);
    gurobi_env->set(GRB_IntParam_Threads, 1);
    gurobi_env->start();
  }

  std::cout.setf(std::ios::unitbuf);
  std::vector<std::string> roots = {"instances/synthetic/baseline", "instances/benchmarks"};
  if (!std::filesystem::exists("instances")) {
    roots = {"../../../../instances/synthetic/baseline", "../../../../instances/benchmarks"};
  }

  std::vector<std::filesystem::path> instance_dirs;
  if (!filter_instance.empty()) {
    std::filesystem::path inst_path(filter_instance);
    if (std::filesystem::exists(inst_path)) {
      instance_dirs.push_back(inst_path);
    }
  } else {
    instance_dirs = collect_instances(roots);
  }
  if (instance_dirs.empty()) {
    std::cout << "No instances found.\n";
    return 0;
  }

  std::filesystem::path runtime_dir = std::filesystem::path("l2") / "runtime";
  std::filesystem::path error_dir = std::filesystem::path("l2") / "errors";
  std::filesystem::create_directories(runtime_dir);
  std::filesystem::create_directories(error_dir);

  std::mt19937 rng(0);

  for (const auto& dir : instance_dirs) {
    if (!matches_instance(dir, filter_instance)) {
      continue;
    }
    std::vector<std::filesystem::path> amb_files;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".amb") {
        if (matches_amb(entry.path(), filter_amb)) {
          amb_files.push_back(entry.path());
        }
      }
    }
    if (amb_files.empty()) {
      continue;
    }

    for (const auto& amb_path : amb_files) {
      std::cout << "Processing " << dir.string() << " ["
                << amb_path.filename().string() << "]\n";
      std::cout << "  L2: starting\n";
      auto mdp_path = dir / "nominal.mdp";
      mdp::AmbiguousMDP data = mdp::load_mdp_amb(mdp_path.string(), amb_path.string());
      const int S = data.mdp.n_states;
      const int A = data.mdp.n_actions;
      if (a10_only && A != 10) {
        std::cout << "Skipping A=" << A << " for " << dir.string() << "\n";
        continue;
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

      std::string base_name = sanitize_name(dir.string());
      std::string amb_name = amb_path.stem().string();
      std::string suffix = run_all ? "" : ("__" + solver);
      std::filesystem::path runtime_path = runtime_dir / (base_name + "__" + amb_name + suffix + ".txt");
      std::filesystem::path error_path = error_dir / (base_name + "__" + amb_name + suffix + ".log");
      if (std::filesystem::exists(runtime_path)) {
        std::cout << "Skipping existing " << runtime_path.string() << "\n";
        continue;
      }

      std::ofstream runtime_out(runtime_path);
      std::ofstream error_out(error_path);

      std::vector<double> fast_times;
      std::vector<double> cplex_times;
      std::vector<double> gurobi_times;
      std::vector<double> mosek_times;
      if (run_fast) {
        fast_times.reserve(100);
      }
      if (run_cplex) {
        cplex_times.reserve(100);
      }
      if (run_gurobi) {
        gurobi_times.reserve(100);
      }
      if (run_mosek) {
        mosek_times.reserve(100);
      }

      error_out << "instance=" << dir.string() << "\n";
      error_out << "amb=" << amb_path.filename().string() << "\n";
      if (!run_all) {
        error_out << "solver=" << solver << "\n";
      }

      double sum_abs = 0.0;
      double max_abs = 0.0;
      double sum_abs_cplex = 0.0;
      double max_abs_cplex = 0.0;
      double sum_abs_gurobi = 0.0;
      double max_abs_gurobi = 0.0;
      int deviation_count = 0;
      int deviation_count_cplex = 0;
      int deviation_count_gurobi = 0;

      std::uniform_int_distribution<int> state_dist(0, S - 1);
      std::uniform_int_distribution<int> action_dist(0, A - 1);

      for (int sample = 0; sample < 100; ++sample) {
        std::cout << "    sample " << (sample + 1) << "/100\n";
        int s = state_dist(rng);
        int a_sample = action_dist(rng);

        std::vector<double> rewards_sa(A * S, 0.0);
        std::vector<double> transitions_sa(A * S, 0.0);
        std::vector<double> sigma_sa(A * S, 1.0);
        for (int a = 0; a < A; ++a) {
          for (int sp = 0; sp < S; ++sp) {
            int idx_global = mdp::index(s, a, sp, S, A);
            int idx_local = a * S + sp;
            rewards_sa[idx_local] = data.mdp.rewards[idx_global];
            transitions_sa[idx_local] = data.mdp.transitions[idx_global];
            sigma_sa[idx_local] = data.amb.sigma_l2[idx_global];
          }
        }

        fast_bellman::BellmanStateInput bellman_in;
        bellman_in.n_states = S;
        bellman_in.n_actions = A;
        bellman_in.discount = data.mdp.discount;
        bellman_in.kappa = data.amb.kappa_l2;
        bellman_in.epsilon = 1e-6;
        bellman_in.lower_bound = -1.0;
        bellman_in.upper_bound = -1.0;
        bellman_in.v = V;
        bellman_in.rewards = rewards_sa;
        bellman_in.transitions = transitions_sa;
        bellman_in.sigma = sigma_sa;

        bool ok_fast = false;
        bool ok_cplex = false;
        bool ok_gurobi = false;
        bool ok_mosek = false;
        double val_fast = 0.0;
        double val_cplex = 0.0;
        double val_gurobi = 0.0;
        double val_mosek = 0.0;

        auto fast_proj = [](const fast_bellman::ProjectionInput& in) {
          fast_l2::ProjectionInput proj{*in.pbar, *in.b, *in.weights, in.beta};
          return fast_l2::solve_projection_problem(proj).objective;
        };

        if (run_fast) {
          try {
            auto t0 = std::chrono::steady_clock::now();
            val_fast = fast_bellman::solve_robust_bellman_state(bellman_in, fast_proj);
            auto t1 = std::chrono::steady_clock::now();
            std::chrono::duration<double, std::micro> dt = t1 - t0;
            fast_times.push_back(dt.count());
            ok_fast = true;
          } catch (const std::exception& exc) {
            error_out << "exception s=" << s << " a=" << a_sample << " fast "
                      << exc.what() << "\n";
          }
        }

        if (run_cplex) {
          try {
            auto t0 = std::chrono::steady_clock::now();
            cplex_l2_bellman::BellmanInput bench_in;
            bench_in.n_states = S;
            bench_in.n_actions = A;
            bench_in.discount = data.mdp.discount;
            bench_in.kappa = data.amb.kappa_l2;
            bench_in.v = V;
            bench_in.rewards = rewards_sa;
            bench_in.transitions = transitions_sa;
            bench_in.sigma = sigma_sa;
            val_cplex = cplex_l2_bellman::solve_bellman_state(bench_in, *cplex_env);
            auto t1 = std::chrono::steady_clock::now();
            std::chrono::duration<double, std::micro> dt = t1 - t0;
            cplex_times.push_back(dt.count());
            ok_cplex = true;
          } catch (const std::exception& exc) {
            error_out << "exception s=" << s << " a=" << a_sample << " cplex "
                      << exc.what() << "\n";
          }
        }

        if (run_gurobi) {
          try {
            auto t0 = std::chrono::steady_clock::now();
            gurobi_l2_bellman::BellmanInput bench_in;
            bench_in.n_states = S;
            bench_in.n_actions = A;
            bench_in.discount = data.mdp.discount;
            bench_in.kappa = data.amb.kappa_l2;
            bench_in.v = V;
            bench_in.rewards = rewards_sa;
            bench_in.transitions = transitions_sa;
            bench_in.sigma = sigma_sa;
            val_gurobi = gurobi_l2_bellman::solve_bellman_state(bench_in, *gurobi_env);
            auto t1 = std::chrono::steady_clock::now();
            std::chrono::duration<double, std::micro> dt = t1 - t0;
            gurobi_times.push_back(dt.count());
            ok_gurobi = true;
          } catch (const std::exception& exc) {
            error_out << "exception s=" << s << " a=" << a_sample << " gurobi "
                      << exc.what() << "\n";
          }
        }

        if (run_mosek) {
          try {
            auto t0 = std::chrono::steady_clock::now();
            mosek_l2_bellman::BellmanInput bench_in;
            bench_in.n_states = S;
            bench_in.n_actions = A;
            bench_in.discount = data.mdp.discount;
            bench_in.kappa = data.amb.kappa_l2;
            bench_in.v = V;
            bench_in.rewards = rewards_sa;
            bench_in.transitions = transitions_sa;
            bench_in.sigma = sigma_sa;
            val_mosek = mosek_l2_bellman::solve_bellman_state(bench_in);
            auto t1 = std::chrono::steady_clock::now();
            std::chrono::duration<double, std::micro> dt = t1 - t0;
            mosek_times.push_back(dt.count());
            ok_mosek = true;
          } catch (const std::exception& exc) {
            error_out << "exception s=" << s << " a=" << a_sample << " mosek "
                      << exc.what() << "\n";
          }
        }

        if (ok_fast && ok_mosek) {
          double diff = std::abs(val_fast - val_mosek);
          sum_abs += diff;
          max_abs = std::max(max_abs, diff);
          deviation_count += 1;
        }
        if (ok_fast && ok_cplex) {
          double diff = std::abs(val_fast - val_cplex);
          sum_abs_cplex += diff;
          max_abs_cplex = std::max(max_abs_cplex, diff);
          deviation_count_cplex += 1;
        }
        if (ok_fast && ok_gurobi) {
          double diff = std::abs(val_fast - val_gurobi);
          sum_abs_gurobi += diff;
          max_abs_gurobi = std::max(max_abs_gurobi, diff);
          deviation_count_gurobi += 1;
        }
      }

      if (deviation_count > 0) {
        error_out << "avg_abs_fast_mosek=" << (sum_abs / deviation_count) << "\n";
        error_out << "max_abs_fast_mosek=" << max_abs << "\n";
      }
      if (deviation_count_cplex > 0) {
        error_out << "avg_abs_fast_cplex=" << (sum_abs_cplex / deviation_count_cplex) << "\n";
        error_out << "max_abs_fast_cplex=" << max_abs_cplex << "\n";
      }
      if (deviation_count_gurobi > 0) {
        error_out << "avg_abs_fast_gurobi=" << (sum_abs_gurobi / deviation_count_gurobi) << "\n";
        error_out << "max_abs_fast_gurobi=" << max_abs_gurobi << "\n";
      }

      auto mean = [](const std::vector<double>& v) {
        double total = 0.0;
        for (double x : v) {
          total += x;
        }
        return v.empty() ? 0.0 : total / static_cast<double>(v.size());
      };

      runtime_out << "instance=" << dir.string() << "\n";
      runtime_out << "amb=" << amb_path.filename().string() << "\n";
      if (!run_all) {
        runtime_out << "solver=" << solver << "\n";
      }
      if (run_fast && !fast_times.empty()) {
        double fast_mean = mean(fast_times);
        double fast_median = quantile(fast_times, 0.5);
        double fast_q1 = quantile(fast_times, 0.25);
        double fast_q3 = quantile(fast_times, 0.75);
        runtime_out << "fast_mean_us=" << fast_mean << "\n";
        runtime_out << "fast_median_us=" << fast_median << "\n";
        runtime_out << "fast_iqr_us=" << (fast_q3 - fast_q1) << "\n";
      }
      if (run_cplex && !cplex_times.empty()) {
        double cplex_mean = mean(cplex_times);
        double cplex_median = quantile(cplex_times, 0.5);
        double cplex_q1 = quantile(cplex_times, 0.25);
        double cplex_q3 = quantile(cplex_times, 0.75);
        runtime_out << "cplex_mean_us=" << cplex_mean << "\n";
        runtime_out << "cplex_median_us=" << cplex_median << "\n";
        runtime_out << "cplex_iqr_us=" << (cplex_q3 - cplex_q1) << "\n";
      }
      if (run_gurobi && !gurobi_times.empty()) {
        double gurobi_mean = mean(gurobi_times);
        double gurobi_median = quantile(gurobi_times, 0.5);
        double gurobi_q1 = quantile(gurobi_times, 0.25);
        double gurobi_q3 = quantile(gurobi_times, 0.75);
        runtime_out << "gurobi_mean_us=" << gurobi_mean << "\n";
        runtime_out << "gurobi_median_us=" << gurobi_median << "\n";
        runtime_out << "gurobi_iqr_us=" << (gurobi_q3 - gurobi_q1) << "\n";
      }
      if (run_mosek && !mosek_times.empty()) {
        double mosek_mean = mean(mosek_times);
        double mosek_median = quantile(mosek_times, 0.5);
        double mosek_q1 = quantile(mosek_times, 0.25);
        double mosek_q3 = quantile(mosek_times, 0.75);
        runtime_out << "mosek_mean_us=" << mosek_mean << "\n";
        runtime_out << "mosek_median_us=" << mosek_median << "\n";
        runtime_out << "mosek_iqr_us=" << (mosek_q3 - mosek_q1) << "\n";
      }
    }
  }

  if (cplex_env) {
    cplex_env->end();
  }
  return 0;
}
