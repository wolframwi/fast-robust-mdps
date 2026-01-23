# Benchmark Documentation

This document records the sources, construction choices, and assumptions for
the benchmark MDP instances in `instances/benchmarks`.

## File formats

Each instance folder contains:

- `nominal.mdp`: nominal MDP in the agreed dense format.
- `mild.amb`, `moderate.amb`, `severe.amb`: ambiguity files.
- `generate.py`: per-instance generator that re-creates the files.

The shared helpers live in `instances/benchmarks/benchmark_generation.py`, and
the per-instance `generate.py` scripts are invoked via
`instances/benchmarks/generate_all.sh`.

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

Initial distributions are included directly in each `nominal.mdp`. For
instance-specific start-state assumptions, see each folder's `description.md`.

## Ambiguity construction (all instances)

Budgets are set using the same rules across instances:

- L1 budgets: `kappa_L1` in `{0.05, 0.10, 0.20}` for mild/moderate/severe.
- L2 budgets: `kappa_L2 = kappa_L1^2`.
- KL/Burg: `kappa_KL = kappa_BURG = kappa_L1^2 / 2` (Pinsker).
- Sigma weights are uniform (`sigma = 1`) for L1/L2.

All ambiguity files follow the agreed minimal format:

```
L1   kappa_L1
L2   kappa_L2
KL   kappa_KL
BURG kappa_BURG
SIGMA_L1
... S*A lines of length S ...
SIGMA_L2
... S*A lines of length S ...
```

## Sources and assumptions by instance

### Gymnasium (toy_text)

These instances are generated directly from Gymnasium transition tables or
full enumeration of the Gym rules.
Initial distributions follow Gymnasium's `env.unwrapped.isd` for each domain.

- `blackjack` (Blackjack-v1)
  - Source: Gymnasium `gymnasium.envs.toy_text.blackjack`.
  - Construction: exact enumeration of player and dealer dynamics with
    infinite deck, `natural=False`, `sab=False`.
  - State space: player sum in `[4, 21]`, dealer showing in `[1, 10]`,
    usable ace in `{0,1}`, plus terminal win/lose/draw states.
  - Actions: stick (0), hit (1).

- `cliffwalking` (CliffWalking-v0)
  - Source: Gymnasium `CliffWalking-v0` transition table.
  - Rewards: per Gym default (step cost `-1`, cliff `-100`).
  - Actions: 4 cardinal moves; terminal state is absorbing.

- `frozenlake4x4`, `frozenlake8x8` (FrozenLake-v1)
  - Source: Gymnasium `FrozenLake-v1`, slippery (stochastic) dynamics.
  - Map: standard Gymnasium layouts for 4x4 and 8x8.
  - Rewards: 1 on goal, 0 otherwise.

- `taxi` (Taxi-v3)
  - Source: Gymnasium `Taxi-v3` transition table.
  - Rewards: per Gym default (step `-1`, illegal pickup/dropoff `-10`,
    successful dropoff `+20`).

### pymdptoolbox

- `forest50`
  - Source: `pymdptoolbox.example.forest`.
  - Parameters: `S=50`, `p=0.1`, `r1=4`, `r2=2` (library defaults except S).
  - Actions: wait, cut; transition structure follows Puterman-style forest
    management benchmark.

### OpenSpiel (pathfinding game rules)

- `openspiel_grid16`
  - Source: OpenSpiel `pathfinding` game rules (single-agent) from the
    OpenSpiel repository.
  - Construction: 4x4 empty grid with start at (0,0) and destination at
    (3,3), no obstacles.
  - Actions: stay, left, up, right, down (OpenSpiel movement set).
  - Rewards: step reward `-0.01`, solve reward `100`, group reward `100`
    (OpenSpiel defaults). Reaching the goal yields `200` once, and the goal
    state is treated as absorbing.

### Synthetic / textbook benchmarks

These instances use standard textbook-style parameterizations commonly used in
robust MDP and DP literature.

- `chain10`
  - 10-state chain; actions bias left/right (`0.9/0.1` vs `0.1/0.9`).
  - Small reward at left end for left action (0.2) and at right end for
    right action (1.0).

- `riverswim6`, `riverswim20`
  - RiverSwim benchmark (Strehl & Littman 2008).
  - Left action moves left deterministically with a boundary self-loop at
    state 0; right action is stochastic.
  - Rewards: small reward at left edge for left action (0.005) and large
    reward at right edge for right action (1.0).

- `gridworld25`
  - 5x5 gridworld, deterministic moves with wall bounce.
  - Terminal rewards: +1 at (4,4), -1 at (4,0), received on entry; terminals
    are absorbing with zero reward thereafter.
  - Step reward: -0.01 elsewhere.

- `capacity50`
  - Capacity allocation / revenue management MDP with 50 states (0..49).
  - Demand per price modeled as Poisson with actions corresponding to
    prices `[1,2,3,4,5]` and rates `[4.0, 3.0, 2.5, 2.0, 1.5]`.
  - Demand truncated at 5 with residual tail mass.
  - Initial distribution: deterministic at state 49 (full capacity).

- `inventory50`
  - Inventory control with 50 inventory levels and order actions 0..4.
  - Demand distribution: `[0.1, 0.2, 0.3, 0.2, 0.15, 0.05]` for demands 0..5.
  - Costs: order 0.5, holding 0.1, shortage 1.0.
  - Initial distribution: deterministic at inventory level 0.

- `perishable50`
  - Perishable inventory with same demand distribution and order actions.
  - Spoilage rate: 0.2 (20% of inventory lost before demand).
  - Costs: order 0.6, holding 0.1, shortage 1.2.
  - Initial distribution: deterministic at inventory level 0.

- `machine20`
  - Machine replacement with 20 states.
  - Actions: keep (deteriorates state, cost increases) or replace (reset,
    fixed cost -2.0).
  - Keep cost: `-(0.1 + 0.02 * state)`.

## Discount factor

All instances use a discount factor of `0.99`, matching common practice in the
benchmark literature and Gymnasium defaults for the toy_text suite.

## Regeneration

From the repo root:

```bash
. .venv/bin/activate
bash instances/benchmarks/generate_all.sh
```
