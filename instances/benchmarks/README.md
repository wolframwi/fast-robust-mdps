# Benchmark Instances

This folder contains one subfolder per benchmark. Each subfolder includes:

- `nominal.mdp`: nominal MDP definition.
- `mild.amb`, `moderate.amb`, `severe.amb`: ambiguity files.
- `generate.py`: instance-specific generator (imports `benchmark_generation.py`).

Nominal MDP format:

```
nStates nActions discount
INITIAL
... S entries ...
REWARDS
... S*A lines of length S ...
TRANSITIONS
... S*A lines of length S ...
```

## Sources

The generators pull from the following upstream sources:

- Gymnasium (toy_text): `Blackjack-v1`, `CliffWalking-v0`, `FrozenLake-v1`, `Taxi-v3`.
- pymdptoolbox: `forest` benchmark (Puterman-style forest management).
- OpenSpiel source (reference): `pathfinding` rules used for `openspiel_grid16` with a 4x4 grid.

Other synthetic instances are implemented using standard formulations from robust MDP / DP literature:

- chain10, riverswim6/20, gridworld25, capacity50, inventory50, perishable50, machine20.

## Regeneration

From the repo root:

```bash
. .venv/bin/activate
bash instances/benchmarks/generate_all.sh
```
