#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "algorithms/mdp.hpp"
#include "algorithms/homotopy/bellman/bellman.hpp"

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

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--instance" && i + 1 < argc) {
      filter_instance = argv[++i];
    } else if (arg == "--amb" && i + 1 < argc) {
      filter_amb = argv[++i];
    } else if (arg == "--help") {
      std::cout << "Usage: run_l1_homotopy_bellman_runtime [--instance PATH] [--amb PATH|NAME]\n";
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
      std::cout << "  L1: homotopy Bellman\n";
      auto mdp_path = dir / "nominal.mdp";
      mdp::AmbiguousMDP data = mdp::load_mdp_amb(mdp_path.string(), amb_path.string());
      const int S = data.mdp.n_states;
      const int A = data.mdp.n_actions;

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
      std::filesystem::path runtime_path =
          runtime_dir / (base_name + "__" + amb_name + ".txt");
      std::filesystem::path error_path =
          error_dir / (base_name + "__" + amb_name + ".log");
      if (std::filesystem::exists(runtime_path)) {
        std::cout << "Skipping existing " << runtime_path.string() << "\n";
        continue;
      }

      std::ofstream runtime_out(runtime_path);
      std::ofstream error_out(error_path);
      error_out << "instance=" << dir.string() << "\n";
      error_out << "amb=" << amb_path.filename().string() << "\n";

      std::vector<double> homotopy_times;
      homotopy_times.reserve(100);

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
            sigma_sa[idx_local] = data.amb.sigma_l1[idx_global];
          }
        }

        homotopy_l1_bellman::BellmanInput homotopy_in;
        homotopy_in.n_states = S;
        homotopy_in.n_actions = A;
        homotopy_in.discount = data.mdp.discount;
        homotopy_in.kappa = data.amb.kappa_l1;
        homotopy_in.v = V;
        homotopy_in.rewards = rewards_sa;
        homotopy_in.transitions = transitions_sa;
        homotopy_in.sigma = sigma_sa;

        bool ok_homotopy = false;
        double val_homotopy = 0.0;

        try {
          auto t0 = std::chrono::steady_clock::now();
          val_homotopy = homotopy_l1_bellman::solve_bellman_state(homotopy_in);
          auto t1 = std::chrono::steady_clock::now();
          std::chrono::duration<double, std::micro> dt = t1 - t0;
          homotopy_times.push_back(dt.count());
          ok_homotopy = true;
        } catch (const std::exception& exc) {
          error_out << "exception s=" << s << " a=" << a_sample << " homotopy "
                    << exc.what() << "\n";
        }

      }

      auto mean = [](const std::vector<double>& v) {
        double total = 0.0;
        for (double x : v) {
          total += x;
        }
        return v.empty() ? 0.0 : total / static_cast<double>(v.size());
      };

      double homotopy_mean = mean(homotopy_times);
      double homotopy_median = quantile(homotopy_times, 0.5);
      double homotopy_q1 = quantile(homotopy_times, 0.25);
      double homotopy_q3 = quantile(homotopy_times, 0.75);

      runtime_out << "instance=" << dir.string() << "\n";
      runtime_out << "amb=" << amb_path.filename().string() << "\n";
      runtime_out << "homotopy_mean_us=" << homotopy_mean << "\n";
      runtime_out << "homotopy_median_us=" << homotopy_median << "\n";
      runtime_out << "homotopy_iqr_us=" << (homotopy_q3 - homotopy_q1) << "\n";
    }
  }

  return 0;
}
