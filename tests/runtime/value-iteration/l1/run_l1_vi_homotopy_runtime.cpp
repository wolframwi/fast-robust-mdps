#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "algorithms/mdp.hpp"
#include "algorithms/homotopy/bellman/bellman.hpp"
#include "algorithms/value_iteration.hpp"

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

} // namespace

int main(int argc, char** argv) {
  std::string filter_instance;
  std::string filter_amb;
  bool a10_only = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--instance" && i + 1 < argc) {
      filter_instance = argv[++i];
    } else if (arg == "--amb" && i + 1 < argc) {
      filter_amb = argv[++i];
    } else if (arg == "--a10-only") {
      a10_only = true;
    } else if (arg == "--help") {
      std::cout
          << "Usage: run_l1_vi_homotopy_runtime [--instance PATH] [--amb PATH|NAME] "
             "[--a10-only]\n";
      return 0;
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      return 1;
    }
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

  std::filesystem::path runtime_dir = std::filesystem::path("l1") / "runtime_homotopy";
  std::filesystem::path error_dir = std::filesystem::path("l1") / "errors_homotopy";
  std::filesystem::create_directories(runtime_dir);
  std::filesystem::create_directories(error_dir);

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
      auto mdp_path = dir / "nominal.mdp";
      mdp::AmbiguousMDP data = mdp::load_mdp_amb(mdp_path.string(), amb_path.string());
      const int S = data.mdp.n_states;
      const int A = data.mdp.n_actions;
      if (a10_only && A != 10) {
        std::cout << "Skipping A=" << A << " for " << dir.string() << "\n";
        continue;
      }

      std::string base_name = sanitize_name(dir.string());
      std::string amb_name = amb_path.stem().string();
      std::filesystem::path runtime_path = runtime_dir / (base_name + "__" + amb_name + ".txt");
      std::filesystem::path error_path = error_dir / (base_name + "__" + amb_name + ".log");
      if (std::filesystem::exists(runtime_path)) {
        std::cout << "Skipping existing " << runtime_path.string() << "\n";
        continue;
      }

      std::cout << "Processing " << dir.string() << " [" << amb_path.filename().string()
                << "]\n";
      std::cout << "  L1: homotopy value iteration\n";

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
            sigma_by_state[s][idx_local] = data.amb.sigma_l1[idx_global];
          }
        }
      }

      std::ofstream runtime_out(runtime_path);
      std::ofstream error_out(error_path);
      error_out << "instance=" << dir.string() << "\n";
      error_out << "amb=" << amb_path.filename().string() << "\n";

      int homotopy_iterations = -1;
      double homotopy_time_us = 0.0;
      bool homotopy_ok = false;

      value_iteration::Result homotopy_result;
      try {
        std::cout << "    starting homotopy value iteration\n";
        auto homotopy_op = [&](int s, const std::vector<double>& v) {
          homotopy_l1_bellman::BellmanInput in;
          in.n_states = S;
          in.n_actions = A;
          in.discount = data.mdp.discount;
          in.kappa = data.amb.kappa_l1;
          in.v = v;
          in.rewards = rewards_by_state[s];
          in.transitions = transitions_by_state[s];
          in.sigma = sigma_by_state[s];
          return homotopy_l1_bellman::solve_bellman_state(in);
        };
        auto t0 = std::chrono::steady_clock::now();
        homotopy_result = value_iteration::solve(S, homotopy_op, 1e-4, &std::cout, 25);
        auto t1 = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::micro> dt = t1 - t0;
        homotopy_time_us = dt.count();
        homotopy_iterations = homotopy_result.iterations;
        homotopy_ok = true;
      } catch (const std::exception& exc) {
        error_out << "exception value_iteration homotopy " << exc.what() << "\n";
      }

      runtime_out << "instance=" << dir.string() << "\n";
      runtime_out << "amb=" << amb_path.filename().string() << "\n";
      runtime_out << "homotopy_vi_iterations=" << homotopy_iterations << "\n";
      runtime_out << "homotopy_vi_s=" << (homotopy_time_us * 1e-6) << "\n";
      runtime_out << "homotopy_failed=" << (homotopy_ok ? 0 : 1) << "\n";
    }
  }

  return 0;
}
