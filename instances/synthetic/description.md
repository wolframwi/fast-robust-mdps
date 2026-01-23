# Synthetic instances

This folder contains synthetic MDP instances used for scaling and stress tests.

Structure:
- Instances are grouped by generator family (e.g., `baseline`).
- Each instance folder is named by size (S, A) and replicate index.
- Each instance includes an ambiguity file (e.g., `moderate.amb`) and the
  corresponding MDP data files used by the solvers/benchmarks.

These instances are used primarily for runtime and scaling experiments.
