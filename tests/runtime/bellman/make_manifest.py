#!/usr/bin/env python3

import argparse
import os
import re
import sys
from pathlib import Path
from typing import Optional


METHOD_SOLVERS = {
    "l1": ["fast", "cplex", "gurobi", "mosek", "homotopy"],
    "l2": ["fast", "cplex", "gurobi", "mosek"],
    "kl": ["fast", "mosek"],
    "burg": ["fast", "mosek"],
}


def find_instances(root: Path):
    for dirpath, _, filenames in os.walk(root):
        if "nominal.mdp" in filenames:
            yield Path(dirpath)


def parse_a_from_path(path: Path):
    match = re.search(r"/A=(\d+)(/|$)", str(path))
    if match:
        return int(match.group(1))
    return None


def parse_a_from_mdp(mdp_path: Path):
    try:
        with mdp_path.open("r", encoding="utf-8", errors="ignore") as f:
            lines = []
            for _ in range(20):
                line = f.readline()
                if not line:
                    break
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                lines.append(line)
                if len(lines) >= 5:
                    break
    except OSError:
        return None

    patterns = [
        r"n_actions\s*=\s*(\d+)",
        r"num_actions\s*=\s*(\d+)",
        r"actions\s*=\s*(\d+)",
        r"\bA\s*=\s*(\d+)\b",
    ]
    for line in lines:
        for pat in patterns:
            m = re.search(pat, line)
            if m:
                return int(m.group(1))
        ints = [int(x) for x in re.findall(r"\b\d+\b", line)]
        if len(ints) >= 2:
            return ints[1]
    return None


def instance_a_value(instance_dir: Path):
    a_val = parse_a_from_path(instance_dir)
    if a_val is not None:
        return a_val
    mdp_path = instance_dir / "nominal.mdp"
    return parse_a_from_mdp(mdp_path)


def sanitize_name(path: str) -> str:
    out = []
    for c in path:
        if c in {"/", "\\", ":", " "}:
            out.append("_")
        elif c == ".":
            continue
        else:
            out.append(c)
    return "".join(out)



def resolve_instances_root(explicit_root: Optional[str]) -> Path:
    if explicit_root:
        return Path(explicit_root).expanduser().resolve()
    env_root = os.environ.get("INSTANCES_ROOT")
    if env_root:
        return Path(env_root).expanduser().resolve()
    base_dir = Path(__file__).resolve().parent
    local_instances = base_dir / "instances"
    if local_instances.exists():
        return local_instances
    return (base_dir / "../../../../instances").resolve()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--instances-root",
        help="Root directory containing synthetic/ and benchmarks/",
        default=None,
    )
    args = parser.parse_args()

    instances_root = resolve_instances_root(args.instances_root)
    roots = [
        instances_root / "synthetic" / "baseline",
        instances_root / "benchmarks",
    ]
    missing_roots = [str(root) for root in roots if not root.exists()]
    if missing_roots:
        print(
            "Error: missing instance roots:\n  " + "\n  ".join(missing_roots),
            file=sys.stderr,
        )
        print(
            "Set --instances-root or INSTANCES_ROOT to the directory that contains "
            "'synthetic/' and 'benchmarks/'.",
            file=sys.stderr,
        )
        return 1

    lines = []
    for root in roots:
        if not root.exists():
            continue
        for inst_dir in find_instances(root):
            amb_files = sorted(p for p in inst_dir.iterdir() if p.suffix == ".amb")
            if not amb_files:
                continue
            for amb_path in amb_files:
                for method, solvers in METHOD_SOLVERS.items():
                    for solver in solvers:
                        lines.append(
                            f"{method}\t{solver}\t{inst_dir}\t{amb_path}"
                        )

    lines.sort()
    for line in lines:
        print(line)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
