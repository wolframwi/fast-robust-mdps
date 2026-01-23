#include "mdp.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace mdp {
namespace {

template <typename T>
void read_or_throw(std::istream& in, T& value, const std::string& context) {
  if (!(in >> value)) {
    throw std::runtime_error("Failed to read " + context);
  }
}

void read_marker(std::istream& in, const std::string& marker) {
  std::string token;
  read_or_throw(in, token, marker);
  if (token != marker) {
    throw std::runtime_error("Expected marker '" + marker + "', got '" + token + "'");
  }
}

void read_vector(std::istream& in, std::vector<double>& out, int count, const std::string& context) {
  out.resize(count);
  for (int i = 0; i < count; ++i) {
    read_or_throw(in, out[i], context);
  }
}

} // namespace

AmbiguousMDP load_mdp_amb(const std::string& mdp_path, const std::string& amb_path) {
  AmbiguousMDP result;

  {
    std::ifstream in(mdp_path);
    if (!in) {
      throw std::runtime_error("Unable to open MDP file: " + mdp_path);
    }
    read_or_throw(in, result.mdp.n_states, "nStates");
    read_or_throw(in, result.mdp.n_actions, "nActions");
    read_or_throw(in, result.mdp.discount, "discount");

    read_marker(in, "INITIAL");
    read_vector(in, result.mdp.initial, result.mdp.n_states, "initial distribution");

    read_marker(in, "REWARDS");
    int total = result.mdp.n_states * result.mdp.n_actions * result.mdp.n_states;
    read_vector(in, result.mdp.rewards, total, "rewards");

    read_marker(in, "TRANSITIONS");
    read_vector(in, result.mdp.transitions, total, "transitions");
  }

  {
    std::ifstream in(amb_path);
    if (!in) {
      throw std::runtime_error("Unable to open ambiguity file: " + amb_path);
    }
    std::string label;
    read_or_throw(in, label, "L1 label");
    if (label != "L1") {
      throw std::runtime_error("Expected L1 line in ambiguity file.");
    }
    read_or_throw(in, result.amb.kappa_l1, "kappa_L1");

    read_or_throw(in, label, "L2 label");
    if (label != "L2") {
      throw std::runtime_error("Expected L2 line in ambiguity file.");
    }
    read_or_throw(in, result.amb.kappa_l2, "kappa_L2");

    read_or_throw(in, label, "KL label");
    if (label != "KL") {
      throw std::runtime_error("Expected KL line in ambiguity file.");
    }
    read_or_throw(in, result.amb.kappa_kl, "kappa_KL");

    read_or_throw(in, label, "BURG label");
    if (label != "BURG") {
      throw std::runtime_error("Expected BURG line in ambiguity file.");
    }
    read_or_throw(in, result.amb.kappa_burg, "kappa_BURG");

    read_marker(in, "SIGMA_L1");
    int total = result.mdp.n_states * result.mdp.n_actions * result.mdp.n_states;
    read_vector(in, result.amb.sigma_l1, total, "sigma_L1");

    read_marker(in, "SIGMA_L2");
    read_vector(in, result.amb.sigma_l2, total, "sigma_L2");
  }

  return result;
}

} // namespace mdp
