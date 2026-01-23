# Runtime tests: value iteration

This folder contains runtime benchmarks for value-iteration loops using the
robust Bellman operators.

Contents:
- Benchmark drivers that iterate the Bellman update over synthetic and
  benchmark instances.
- Output data/plots summarizing total runtime and iteration counts.

Typical workflow:
- Build the value-iteration benchmark binaries.
- Run the drivers to collect timings.
- Aggregate results into plots or tables.
