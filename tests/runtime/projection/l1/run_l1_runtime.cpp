#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "gurobi_c++.h"
#include <ilcplex/ilocplex.h>

#include "algorithms/mdp.hpp"
#include "algorithms/projection_utils.hpp"
#include "algorithms/cplex/l1/projection/project.hpp"
#include "algorithms/fast/l1/projection/project.hpp"
#include "algorithms/gurobi/l1/projection/project.hpp"
#include "algorithms/mosek/l1/projection/project.hpp"

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

double dot(const std::vector<double>& a, const std::vector<double>& b) {
  double total = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    total += a[i] * b[i];
  }
  return total;
}

double quantile(std::vector<double> values, double q) {
  if (values.empty()) {
    return 0.0;
  }
  double pos = q * (values.size() - 1);
  size_t idx = static_cast<size_t>(pos);
  std::nth_element(values.begin(), values.begin() + idx, values.end());
  double v = values[idx];
  return v;
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
          << "Usage: run_l1_runtime [--instance PATH] [--amb PATH|NAME] "
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

  std::filesystem::path runtime_dir = std::filesystem::path("l1") / "runtime";
  std::filesystem::path error_dir = std::filesystem::path("l1") / "errors";
  std::filesystem::create_directories(runtime_dir);
  std::filesystem::create_directories(error_dir);

  std::unique_ptr<GRBEnv> gurobi_env;
  if (run_gurobi) {
    gurobi_env = std::make_unique<GRBEnv>(true);
    gurobi_env->set(GRB_IntParam_OutputFlag, 0);
    gurobi_env->set(GRB_IntParam_Threads, 1);
    gurobi_env->start();
  }
  std::unique_ptr<IloEnv> cplex_env;
  if (run_cplex) {
    cplex_env = std::make_unique<IloEnv>();
  }

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
      std::cout << "Processing " << dir.string() << " [" << amb_path.filename().string() << "]\n";
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
      double min_b = *std::min_element(V.begin(), V.end());

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
        fast_times.reserve(S * A);
      }
      if (run_cplex) {
        cplex_times.reserve(S * A);
      }
      if (run_gurobi) {
        gurobi_times.reserve(S * A);
      }
      if (run_mosek) {
        mosek_times.reserve(S * A);
      }

      error_out << "instance=" << dir.string() << "\n";
      error_out << "amb=" << amb_path.filename().string() << "\n";
      if (!run_all) {
        error_out << "solver=" << solver << "\n";
      }

      double sum_obj = 0.0;
      double max_obj = 0.0;
      double sum_obj_cplex = 0.0;
      double max_obj_cplex = 0.0;
      double sum_obj_gurobi = 0.0;
      double max_obj_gurobi = 0.0;
      int deviation_count = 0;
      int deviation_count_cplex = 0;
      int deviation_count_gurobi = 0;

      for (int s = 0; s < S; ++s) {
        for (int a = 0; a < A; ++a) {
          std::vector<double> pbar(S, 0.0);
          std::vector<double> weights(S, 1.0);
          for (int sp = 0; sp < S; ++sp) {
            int idx = mdp::index(s, a, sp, S, A);
            pbar[sp] = data.mdp.transitions[idx];
            weights[sp] = data.amb.sigma_l1[idx];
          }

          double baseline = dot(pbar, V);
          double beta = baseline;
          if (baseline > min_b) {
            beta = 0.5 * (baseline + min_b);
          }

          double obj_fast = 0.0;
          double obj_mosek = 0.0;
          double obj_cplex = 0.0;
          double obj_gurobi = 0.0;
          std::vector<double> p_mosek;
          std::vector<double> p_cplex;
          std::vector<double> p_gurobi;
          bool ok_fast = false;
          bool ok_cplex = false;
          bool ok_gurobi = false;
          bool ok_mosek = false;

          if (run_fast) {
            try {
              fast_l1::ProjectionInput in_fast{pbar, V, weights, beta};
              auto t0 = std::chrono::steady_clock::now();
              auto result = fast_l1::solve_projection_problem(in_fast);
              auto t1 = std::chrono::steady_clock::now();
              std::chrono::duration<double, std::micro> dt = t1 - t0;
              fast_times.push_back(dt.count());
              obj_fast = result.objective;
              ok_fast = true;
            } catch (const std::exception& exc) {
              error_out << "exception s=" << s << " a=" << a << " fast " << exc.what() << "\n";
            }
          }

          if (run_mosek) {
            try {
              mosek_l1::ProjectionInput in_mosek{pbar, V, weights, beta};
              auto t0 = std::chrono::steady_clock::now();
              auto result = mosek_l1::solve_projection_problem(in_mosek);
              auto t1 = std::chrono::steady_clock::now();
              std::chrono::duration<double, std::micro> dt = t1 - t0;
              mosek_times.push_back(dt.count());
              p_mosek = result.p;
              obj_mosek = projection_utils::l1_distance(p_mosek, pbar, weights);
              ok_mosek = true;
            } catch (const std::exception& exc) {
              error_out << "exception s=" << s << " a=" << a << " mosek " << exc.what() << "\n";
            }
          }

          if (run_gurobi) {
            try {
              gurobi_l1::ProjectionInput in_gurobi{pbar, V, weights, beta};
              auto t0 = std::chrono::steady_clock::now();
              auto result = gurobi_l1::solve_projection_problem(in_gurobi, *gurobi_env);
              auto t1 = std::chrono::steady_clock::now();
              std::chrono::duration<double, std::micro> dt = t1 - t0;
              gurobi_times.push_back(dt.count());
              p_gurobi = result.p;
              obj_gurobi = projection_utils::l1_distance(p_gurobi, pbar, weights);
              ok_gurobi = true;
            } catch (const std::exception& exc) {
              error_out << "exception s=" << s << " a=" << a << " gurobi " << exc.what() << "\n";
            }
          }

          if (run_cplex) {
            try {
              cplex_l1::ProjectionInput in_cplex{pbar, V, weights, beta};
              auto t0 = std::chrono::steady_clock::now();
              auto result = cplex_l1::solve_projection_problem(in_cplex, *cplex_env);
              auto t1 = std::chrono::steady_clock::now();
              std::chrono::duration<double, std::micro> dt = t1 - t0;
              cplex_times.push_back(dt.count());
              p_cplex = result.p;
              obj_cplex = projection_utils::l1_distance(p_cplex, pbar, weights);
              ok_cplex = true;
            } catch (const std::exception& exc) {
              error_out << "exception s=" << s << " a=" << a << " cplex " << exc.what() << "\n";
            }
          }

          if (ok_fast && ok_mosek) {
            double obj = std::abs(obj_fast - obj_mosek);
            sum_obj += obj;
            max_obj = std::max(max_obj, obj);
            deviation_count += 1;
          }
          if (ok_fast && ok_gurobi) {
            double obj = std::abs(obj_fast - obj_gurobi);
            sum_obj_gurobi += obj;
            max_obj_gurobi = std::max(max_obj_gurobi, obj);
            deviation_count_gurobi += 1;
          }
          if (ok_fast && ok_cplex) {
            double obj = std::abs(obj_fast - obj_cplex);
            sum_obj_cplex += obj;
            max_obj_cplex = std::max(max_obj_cplex, obj);
            deviation_count_cplex += 1;
          }
        }
      }

      if (deviation_count > 0) {
        error_out << "avg_obj_fast_mosek=" << (sum_obj / deviation_count) << "\n";
        error_out << "max_obj_fast_mosek=" << max_obj << "\n";
      }
      if (deviation_count_gurobi > 0) {
        error_out << "avg_obj_fast_gurobi=" << (sum_obj_gurobi / deviation_count_gurobi) << "\n";
        error_out << "max_obj_fast_gurobi=" << max_obj_gurobi << "\n";
      }
      if (deviation_count_cplex > 0) {
        error_out << "avg_obj_fast_cplex=" << (sum_obj_cplex / deviation_count_cplex) << "\n";
        error_out << "max_obj_fast_cplex=" << max_obj_cplex << "\n";
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
