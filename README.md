# Fast Robust MDPs

This repository contains C++ implementations for robust Markov decision process
(MDP) algorithms, including fast projection operators, robust Bellman updates,
and value-iteration utilities. It also includes synthetic and benchmark
instances plus runtime and correctness tests.

## Repository layout

- `algorithms/`: Core algorithms (projections, Bellman operators, value
  iteration).
- `instances/`: Benchmark and synthetic MDP instances.
- `tests/`: Correctness tests, runtime benchmarks, and profiling utilities.

## Algorithms

- Projection operators for L1, L2, KL, and Burg ambiguity sets, with both
  solver-based (MOSEK/CPLEX/Gurobi) and fast custom implementations.
- Robust Bellman operators using the above ambiguity sets.
- A generic value-iteration loop in `algorithms/value_iteration.hpp`.

Each algorithm folder contains a `description.md` that documents the model and
method.

## Instances

- `instances/benchmarks/` contains standard MDP benchmarks. Each benchmark
  includes `nominal.mdp` and ambiguity files, plus a `description.md`.
- `instances/synthetic/` contains generated synthetic instances used for
  scaling studies.

See `instances/benchmarks/README.md` for detailed benchmark sources and
regeneration notes.

## Tests and benchmarks

- `tests/correctness/`: Unit-style checks for algorithm correctness.
- `tests/runtime/`: Runtime benchmarks for projections, Bellman updates, and
  value iteration (each with a `description.md`).
- `tests/profiling/`: Profiling utilities and scripts.

## Solver dependencies

Some implementations require commercial solvers:
- MOSEK (Fusion API)
- Gurobi
- CPLEX

The fast projection/Bellman implementations do not require these solvers.

## Getting started

There is no single top-level build script in this repository. Most experiments
and benchmarks are driven by the C++ sources under `tests/` and by instance
files under `instances/`. If you want a specific build/run workflow documented,
let me know and I can add a `BUILDING.md` or expand this README.
