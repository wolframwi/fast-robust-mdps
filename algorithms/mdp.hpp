#pragma once

#include <string>
#include <vector>

namespace mdp {

struct MDP {
  int n_states = 0;
  int n_actions = 0;
  double discount = 0.0;
  std::vector<double> initial;     // size S
  std::vector<double> rewards;     // size S*A*S
  std::vector<double> transitions; // size S*A*S
};

struct Ambiguity {
  double kappa_l1 = 0.0;
  double kappa_l2 = 0.0;
  double kappa_kl = 0.0;
  double kappa_burg = 0.0;
  std::vector<double> sigma_l1; // size S*A*S
  std::vector<double> sigma_l2; // size S*A*S
};

struct AmbiguousMDP {
  MDP mdp;
  Ambiguity amb;
};

AmbiguousMDP load_mdp_amb(const std::string& mdp_path, const std::string& amb_path);

inline int index(int s, int a, int sp, int n_states, int n_actions) {
  return (s * n_actions + a) * n_states + sp;
}

} // namespace mdp
