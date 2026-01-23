# Runtime tests: projection

This folder contains runtime benchmarks for the projection operators (L1, L2,
KL, Burg) across solvers and datasets.

Contents:
- Subfolders per divergence (l1, l2, kl, burg) and deviation sweeps.
- `manifest.tsv` enumerates benchmark and synthetic instances used in the
  projection timing runs.
- `make_manifest.py` builds the manifest; `table_summary_*.py` scripts aggregate
  results into tables.
- `doit.pbs` is a batch script for running jobs on a cluster.

Typical workflow:
- Generate or update the manifest.
- Run the benchmark binaries in each subfolder.
- Summarize runtimes with the table scripts.
