#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
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

std::unordered_set<std::string> split_methods(const std::string& methods) {
  std::unordered_set<std::string> out;
  if (methods.empty() || methods == "all") {
    return out;
  }
  size_t start = 0;
  while (start < methods.size()) {
    size_t comma = methods.find(',', start);
    if (comma == std::string::npos) {
      comma = methods.size();
    }
    std::string token = methods.substr(start, comma - start);
    if (!token.empty()) {
      out.insert(token);
    }
    start = comma + 1;
  }
  return out;
}

bool wants_method(const std::unordered_set<std::string>& methods, const std::string& name) {
  return methods.empty() || methods.count(name) > 0;
}

void usage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " --instance <dir> --amb <file> --methods <list|all>"
               " --runtime-out <path> --error-out <path>\n";
}

} // namespace

int main(int argc, char** argv) {
  std::string instance_dir;
  std::string amb_path_arg;
  std::string methods_arg = "all";
  std::string runtime_out_path;
  std::string error_out_path;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--instance" && i + 1 < argc) {
      instance_dir = argv[++i];
    } else if (arg == "--amb" && i + 1 < argc) {
      amb_path_arg = argv[++i];
    } else if (arg == "--methods" && i + 1 < argc) {
      methods_arg = argv[++i];
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
  std::unordered_set<std::string> methods = split_methods(methods_arg);
  const bool want_fast = wants_method(methods, "fast");
  const bool want_cplex = wants_method(methods, "cplex");
  const bool want_gurobi = wants_method(methods, "gurobi");
  const bool want_mosek = wants_method(methods, "mosek");

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
        sigma_by_state[s][idx_local] = data.amb.sigma_l2[idx_global];
      }
    }
  }

  std::ofstream runtime_out(runtime_out_path);
  std::ofstream error_out(error_out_path);
  runtime_out << "instance=" << instance_path.string() << "\n";
  runtime_out << "amb=" << amb_path.filename().string() << "\n";
  error_out << "instance=" << instance_path.string() << "\n";
  error_out << "amb=" << amb_path.filename().string() << "\n";

  auto fast_proj = [](const fast_bellman::ProjectionInput& in) {
    fast_l2::ProjectionInput proj{*in.pbar, *in.b, *in.weights, in.beta};
    return fast_l2::solve_projection_problem(proj).objective;
  };

  bool fast_ok = false;
  bool cplex_ok = false;
  bool gurobi_ok = false;
  bool mosek_ok = false;
  int fast_iterations = -1;
  int cplex_iterations = -1;
  int gurobi_iterations = -1;
  int mosek_iterations = -1;
  double fast_time_us = 0.0;
  double cplex_time_us = 0.0;
  double gurobi_time_us = 0.0;
  double mosek_time_us = 0.0;
  std::vector<double> fast_value;
  std::vector<double> cplex_value;
  std::vector<double> gurobi_value;
  std::vector<double> mosek_value;

  if (want_fast) {
    try {
      std::cout << "starting fast value iteration\n";
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
      value_iteration::Result fast_result = value_iteration::solve(S, fast_op, 1e-4, &std::cout,
                                                                   25, 2500);
      auto t1 = std::chrono::steady_clock::now();
      std::chrono::duration<double, std::micro> dt = t1 - t0;
      fast_time_us = dt.count();
      fast_iterations = fast_result.iterations;
      fast_value = fast_result.value;
      fast_ok = true;
    } catch (const std::exception& exc) {
      error_out << "exception value_iteration fast " << exc.what() << "\n";
    }
    runtime_out << "fast_vi_iterations=" << fast_iterations << "\n";
    runtime_out << "fast_vi_s=" << (fast_time_us * 1e-6) << "\n";
    runtime_out << "fast_failed=" << (fast_ok ? 0 : 1) << "\n";
  }

  if (want_cplex) {
    try {
      std::cout << "starting CPLEX value iteration\n";
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
      value_iteration::Result cplex_result = value_iteration::solve(S, cplex_op, 1e-4, &std::cout,
                                                                    25, 2500);
      auto t1 = std::chrono::steady_clock::now();
      std::chrono::duration<double, std::micro> dt = t1 - t0;
      cplex_time_us = dt.count();
      cplex_iterations = cplex_result.iterations;
      if (want_fast) {
        cplex_value = cplex_result.value;
      }
      cplex_ok = true;
    } catch (const std::exception& exc) {
      error_out << "exception value_iteration cplex " << exc.what() << "\n";
    }
    runtime_out << "cplex_vi_iterations=" << cplex_iterations << "\n";
    runtime_out << "cplex_vi_s=" << (cplex_time_us * 1e-6) << "\n";
    runtime_out << "cplex_failed=" << (cplex_ok ? 0 : 1) << "\n";
  }

  if (want_mosek) {
    try {
      std::cout << "starting MOSEK value iteration\n";
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
      value_iteration::Result mosek_result = value_iteration::solve(S, mosek_op, 1e-4, &std::cout,
                                                                    25, 2500);
      auto t1 = std::chrono::steady_clock::now();
      std::chrono::duration<double, std::micro> dt = t1 - t0;
      mosek_time_us = dt.count();
      mosek_iterations = mosek_result.iterations;
      if (want_fast) {
        mosek_value = mosek_result.value;
      }
      mosek_ok = true;
    } catch (const std::exception& exc) {
      error_out << "exception value_iteration mosek " << exc.what() << "\n";
    }
    runtime_out << "mosek_vi_iterations=" << mosek_iterations << "\n";
    runtime_out << "mosek_vi_s=" << (mosek_time_us * 1e-6) << "\n";
    runtime_out << "mosek_failed=" << (mosek_ok ? 0 : 1) << "\n";
  }

  if (want_gurobi) {
    try {
      std::cout << "starting Gurobi value iteration\n";
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
      value_iteration::Result gurobi_result = value_iteration::solve(S, gurobi_op, 1e-4, &std::cout,
                                                                     25, 2500);
      auto t1 = std::chrono::steady_clock::now();
      std::chrono::duration<double, std::micro> dt = t1 - t0;
      gurobi_time_us = dt.count();
      gurobi_iterations = gurobi_result.iterations;
      if (want_fast) {
        gurobi_value = gurobi_result.value;
      }
      gurobi_ok = true;
    } catch (const std::exception& exc) {
      error_out << "exception value_iteration gurobi " << exc.what() << "\n";
    }
    runtime_out << "gurobi_vi_iterations=" << gurobi_iterations << "\n";
    runtime_out << "gurobi_vi_s=" << (gurobi_time_us * 1e-6) << "\n";
    runtime_out << "gurobi_failed=" << (gurobi_ok ? 0 : 1) << "\n";
  }

  if (want_fast && fast_ok && want_cplex && cplex_ok && !cplex_value.empty()) {
    double diff = 0.0;
    for (int i = 0; i < S; ++i) {
      diff = std::max(diff, std::abs(fast_value[i] - cplex_value[i]));
    }
    runtime_out << "vi_infty_diff_fast_cplex=" << diff << "\n";
  }
  if (want_fast && fast_ok && want_gurobi && gurobi_ok && !gurobi_value.empty()) {
    double diff = 0.0;
    for (int i = 0; i < S; ++i) {
      diff = std::max(diff, std::abs(fast_value[i] - gurobi_value[i]));
    }
    runtime_out << "vi_infty_diff_fast_gurobi=" << diff << "\n";
  }
  if (want_fast && fast_ok && want_mosek && mosek_ok && !mosek_value.empty()) {
    double diff = 0.0;
    for (int i = 0; i < S; ++i) {
      diff = std::max(diff, std::abs(fast_value[i] - mosek_value[i]));
    }
    runtime_out << "vi_infty_diff_fast_mosek=" << diff << "\n";
  }

  return 0;
}
