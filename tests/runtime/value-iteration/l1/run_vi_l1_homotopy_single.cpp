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

void usage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " --instance <dir> --amb <file>"
               " --runtime-out <path> --error-out <path>\n";
}

} // namespace

int main(int argc, char** argv) {
  std::string instance_dir;
  std::string amb_path_arg;
  std::string runtime_out_path;
  std::string error_out_path;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--instance" && i + 1 < argc) {
      instance_dir = argv[++i];
    } else if (arg == "--amb" && i + 1 < argc) {
      amb_path_arg = argv[++i];
    } else if (arg == "--runtime-out" && i + 1 < argc) {
      runtime_out_path = argv[++i];
    } else if (arg == "--error-out" && i + 1 < argc) {
      error_out_path = argv[++i];
    } else {
      usage(argv[0]);
      return 1;
    }
  }

  if (instance_dir.empty() || amb_path_arg.empty() || runtime_out_path.empty() ||
      error_out_path.empty()) {
    usage(argv[0]);
    return 1;
  }

  std::filesystem::path instance_path(instance_dir);
  std::filesystem::path amb_path(amb_path_arg);
  if (!std::filesystem::exists(amb_path)) {
    amb_path = instance_path / amb_path_arg;
  }
  if (!std::filesystem::exists(instance_path) || !std::filesystem::exists(amb_path)) {
    std::cerr << "Instance or ambiguity file not found.\n";
    return 1;
  }

  std::cout.setf(std::ios::unitbuf);
  mdp::AmbiguousMDP data =
      mdp::load_mdp_amb((instance_path / "nominal.mdp").string(), amb_path.string());
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
        sigma_by_state[s][idx_local] = data.amb.sigma_l1[idx_global];
      }
    }
  }

  int homotopy_iterations = -1;
  double homotopy_time_us = 0.0;

  std::ofstream runtime_out(runtime_out_path);
  std::ofstream error_out(error_out_path);
  error_out << "instance=" << instance_path.string() << "\n";
  error_out << "amb=" << amb_path.filename().string() << "\n";
  runtime_out << "instance=" << instance_path.string() << "\n";
  runtime_out << "amb=" << amb_path.filename().string() << "\n";

  try {
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
    value_iteration::Result result = value_iteration::solve(S, homotopy_op, 1e-4, &std::cout, 25);
    auto t1 = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::micro> dt = t1 - t0;
    homotopy_time_us = dt.count();
    homotopy_iterations = result.iterations;
  } catch (const std::exception& exc) {
    error_out << "exception value_iteration homotopy " << exc.what() << "\n";
  }

  runtime_out << "homotopy_vi_iterations=" << homotopy_iterations << "\n";
  runtime_out << "homotopy_vi_s=" << (homotopy_time_us * 1e-6) << "\n";
  return 0;
}
