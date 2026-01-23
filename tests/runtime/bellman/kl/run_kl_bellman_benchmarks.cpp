#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "algorithms/mdp.hpp"
#include "algorithms/fast/bellman/bellman.hpp"
#include "algorithms/fast/kl/projection/project.hpp"
#include "algorithms/mosek/kl/bellman/bellman.hpp"
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

double quantile(std::vector<double> values, double q) {
  if (values.empty()) {
    return 0.0;
  }
  double pos = q * (values.size() - 1);
  size_t idx = static_cast<size_t>(pos);
  std::nth_element(values.begin(), values.begin() + idx, values.end());
  return values[idx];
}

double mean(const std::vector<double>& values) {
  double total = 0.0;
  for (double v : values) {
    total += v;
  }
  return values.empty() ? 0.0 : total / static_cast<double>(values.size());
}

} // namespace

int main() {
  std::cout.setf(std::ios::unitbuf);
  std::vector<std::string> roots = {"instances/benchmarks"};
  if (!std::filesystem::exists("instances")) {
    roots = {"../../../../instances/benchmarks"};
  }

  std::vector<std::filesystem::path> instance_dirs = collect_instances(roots);
  if (instance_dirs.empty()) {
    std::cout << "No instances found.\n";
    return 0;
  }

  std::filesystem::path runtime_dir = "runtime_benchmarks";
  std::filesystem::path error_dir = "errors_benchmarks";
  std::filesystem::create_directories(runtime_dir);
  std::filesystem::create_directories(error_dir);

  std::mt19937 rng(0);
  int mosek_failed_instances = 0;
  int total_instances = 0;

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
      total_instances += 1;
      std::cout << "Processing " << dir.string() << " ["
                << amb_path.filename().string() << "]\n";
      std::cout << "  KL: starting\n";
      auto mdp_path = dir / "nominal.mdp";
      mdp::AmbiguousMDP data = mdp::load_mdp_amb(mdp_path.string(), amb_path.string());
      const int S = data.mdp.n_states;
      const int A = data.mdp.n_actions;

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

      std::vector<std::vector<double>> rewards_by_state(S, std::vector<double>(A * S, 0.0));
      std::vector<std::vector<double>> transitions_by_state(S, std::vector<double>(A * S, 0.0));
      for (int s = 0; s < S; ++s) {
        for (int a = 0; a < A; ++a) {
          for (int sp = 0; sp < S; ++sp) {
            int idx_global = mdp::index(s, a, sp, S, A);
            int idx_local = a * S + sp;
            rewards_by_state[s][idx_local] = data.mdp.rewards[idx_global];
            transitions_by_state[s][idx_local] = transitions_bumped[idx_global];
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

      std::string base_name = sanitize_name(dir.string());
      std::string amb_name = amb_path.stem().string();
      std::filesystem::path runtime_path = runtime_dir / (base_name + "__" + amb_name + ".txt");
      std::filesystem::path error_path = error_dir / (base_name + "__" + amb_name + ".log");
      if (std::filesystem::exists(runtime_path)) {
        std::cout << "Skipping existing " << runtime_path.string() << "\n";
        continue;
      }

      std::ofstream runtime_out(runtime_path);
      std::ofstream error_out(error_path);

      std::vector<double> fast_times;
      std::vector<double> mosek_times;
      fast_times.reserve(S);
      mosek_times.reserve(S);

      error_out << "instance=" << dir.string() << "\n";
      error_out << "amb=" << amb_path.filename().string() << "\n";

      double sum_abs = 0.0;
      double max_abs = 0.0;
      int deviation_count = 0;
      bool mosek_had_error = false;

      for (int s = 0; s < S; ++s) {
        const std::vector<double>& rewards_sa = rewards_by_state[s];
        const std::vector<double>& transitions_sa = transitions_by_state[s];

        fast_bellman::BellmanStateInput bellman_in;
        bellman_in.n_states = S;
        bellman_in.n_actions = A;
        bellman_in.discount = data.mdp.discount;
        bellman_in.kappa = data.amb.kappa_kl;
        bellman_in.epsilon = 1e-6;
        bellman_in.lower_bound = -1.0;
        bellman_in.upper_bound = -1.0;
        bellman_in.v = V;
        bellman_in.rewards = rewards_sa;
        bellman_in.transitions = transitions_sa;

        bool ok_fast = false;
        bool ok_mosek = false;
        double val_fast = 0.0;
        double val_mosek = 0.0;

        auto fast_proj = [](const fast_bellman::ProjectionInput& in) {
          fast_kl::ProjectionInput proj{*in.pbar, *in.b, in.beta};
          return fast_kl::solve_projection_problem(proj).objective;
        };

        try {
          auto t0 = std::chrono::steady_clock::now();
          val_fast = fast_bellman::solve_robust_bellman_state(bellman_in, fast_proj);
          auto t1 = std::chrono::steady_clock::now();
          std::chrono::duration<double, std::micro> dt = t1 - t0;
          fast_times.push_back(dt.count());
          ok_fast = true;
        } catch (const std::exception& exc) {
          error_out << "exception s=" << s << " fast " << exc.what() << "\n";
        }

        try {
          auto t0 = std::chrono::steady_clock::now();
          mosek_kl_bellman::BellmanInput bench_in;
          bench_in.n_states = S;
          bench_in.n_actions = A;
          bench_in.discount = data.mdp.discount;
          bench_in.kappa = data.amb.kappa_kl;
          bench_in.v = V;
          bench_in.rewards = rewards_sa;
          bench_in.transitions = transitions_sa;
          val_mosek = mosek_kl_bellman::solve_bellman_state(bench_in);
          auto t1 = std::chrono::steady_clock::now();
          std::chrono::duration<double, std::micro> dt = t1 - t0;
          mosek_times.push_back(dt.count());
          ok_mosek = true;
        } catch (const std::exception& exc) {
          error_out << "exception s=" << s << " mosek " << exc.what() << "\n";
          mosek_had_error = true;
        }

        if (ok_fast && ok_mosek) {
          double diff = std::abs(val_fast - val_mosek);
          sum_abs += diff;
          max_abs = std::max(max_abs, diff);
          deviation_count += 1;
        }
      }

      if (deviation_count > 0) {
        error_out << "avg_abs_fast_mosek=" << (sum_abs / deviation_count) << "\n";
        error_out << "max_abs_fast_mosek=" << max_abs << "\n";
      }

      double fast_mean = mean(fast_times);
      double mosek_mean = mean(mosek_times);
      double fast_median = quantile(fast_times, 0.5);
      double mosek_median = quantile(mosek_times, 0.5);
      double fast_q1 = quantile(fast_times, 0.25);
      double fast_q3 = quantile(fast_times, 0.75);
      double mosek_q1 = quantile(mosek_times, 0.25);
      double mosek_q3 = quantile(mosek_times, 0.75);

      runtime_out << "instance=" << dir.string() << "\n";
      runtime_out << "amb=" << amb_path.filename().string() << "\n";
      runtime_out << "fast_mean_us=" << fast_mean << "\n";
      runtime_out << "fast_median_us=" << fast_median << "\n";
      runtime_out << "fast_iqr_us=" << (fast_q3 - fast_q1) << "\n";
      runtime_out << "mosek_mean_us=" << mosek_mean << "\n";
      runtime_out << "mosek_median_us=" << mosek_median << "\n";
      runtime_out << "mosek_iqr_us=" << (mosek_q3 - mosek_q1) << "\n";

      auto fast_proj = [](const fast_bellman::ProjectionInput& in) {
        fast_kl::ProjectionInput proj{*in.pbar, *in.b, in.beta};
        return fast_kl::solve_projection_problem(proj).objective;
      };

      int fast_iterations = 0;
      int mosek_iterations = 0;
      double inf_diff = -1.0;
      bool fast_vi_ok = false;
      bool mosek_vi_ok = false;

      value_iteration::Result fast_result;
      try {
        auto fast_op = [&](int s, const std::vector<double>& v) {
          fast_bellman::BellmanStateInput in;
          in.n_states = S;
          in.n_actions = A;
          in.discount = data.mdp.discount;
          in.kappa = data.amb.kappa_kl;
          in.epsilon = 1e-6;
          in.lower_bound = -1.0;
          in.upper_bound = -1.0;
          in.v = v;
          in.rewards = rewards_by_state[s];
          in.transitions = transitions_by_state[s];
          return fast_bellman::solve_robust_bellman_state(in, fast_proj);
        };
        fast_result = value_iteration::solve(S, fast_op, 1e-4);
        fast_iterations = fast_result.iterations;
        fast_vi_ok = true;
      } catch (const std::exception& exc) {
        error_out << "exception value_iteration fast " << exc.what() << "\n";
      }

      value_iteration::Result mosek_result;
      try {
        auto mosek_op = [&](int s, const std::vector<double>& v) {
          mosek_kl_bellman::BellmanInput in;
          in.n_states = S;
          in.n_actions = A;
          in.discount = data.mdp.discount;
          in.kappa = data.amb.kappa_kl;
          in.v = v;
          in.rewards = rewards_by_state[s];
          in.transitions = transitions_by_state[s];
          return mosek_kl_bellman::solve_bellman_state(in);
        };
        mosek_result = value_iteration::solve(S, mosek_op, 1e-4);
        mosek_iterations = mosek_result.iterations;
        mosek_vi_ok = true;
      } catch (const std::exception& exc) {
        error_out << "exception value_iteration mosek " << exc.what() << "\n";
        mosek_had_error = true;
      }

      if (fast_vi_ok && mosek_vi_ok) {
        inf_diff = 0.0;
        for (int i = 0; i < S; ++i) {
          inf_diff = std::max(inf_diff, std::abs(fast_result.value[i] - mosek_result.value[i]));
        }
      }

      runtime_out << "fast_vi_iterations=" << fast_iterations << "\n";
      runtime_out << "mosek_vi_iterations=" << mosek_iterations << "\n";
      runtime_out << "vi_infty_diff=" << inf_diff << "\n";
      runtime_out << "mosek_failed=" << (mosek_had_error ? 1 : 0) << "\n";

      if (mosek_had_error) {
        mosek_failed_instances += 1;
      }
    }
  }

  std::filesystem::path summary_path = runtime_dir / "summary.txt";
  std::ofstream summary_out(summary_path);
  summary_out << "total_instances=" << total_instances << "\n";
  summary_out << "mosek_failed_instances=" << mosek_failed_instances << "\n";

  std::cout << "Summary: total_instances=" << total_instances
            << " mosek_failed_instances=" << mosek_failed_instances << "\n";
  return 0;
}
