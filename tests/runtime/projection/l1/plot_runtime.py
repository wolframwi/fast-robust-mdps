#!/usr/bin/env python3

import os
import re
import statistics
from collections import defaultdict

try:
    import matplotlib.pyplot as plt
except Exception as exc:
    raise SystemExit(f"matplotlib is required: {exc}")


def parse_runtime_file(path):
    data = {}
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or "=" not in line:
                continue
            key, value = line.split("=", 1)
            data[key] = value
    return data


def main():
    base_dir = os.path.dirname(__file__)
    runtime_dir = os.path.join(base_dir, "runtime")

    synthetic = defaultdict(lambda: defaultdict(list))
    for fname in os.listdir(runtime_dir):
        if not fname.endswith(".txt"):
            continue
        data = parse_runtime_file(os.path.join(runtime_dir, fname))
        instance = data.get("instance", "")
        amb = data.get("amb", "")
        if "/synthetic/baseline/" not in instance:
            continue
        m = re.search(r"S=(\d+)/A=(\d+)/rep=(\d+)", instance)
        if not m:
            continue
        s = int(m.group(1))
        a = int(m.group(2))
        synthetic[(s, a)][amb].append(data)

    configs = [("", 10, "l1_projection_runtime.pdf")]

    for title, mode, filename in configs:
        fig, ax = plt.subplots(1, 1, figsize=(5, 4))
        series = {"fast": [], "cplex": [], "gurobi": [], "mosek": []}
        for (s, a), amb_data in synthetic.items():
            if mode == 10 and a != 10:
                continue
            if mode == "eq" and a != s:
                continue
            entries = amb_data.get("moderate.amb", [])
            if not entries:
                continue
            for key, label in [
                ("fast_median_us", "fast"),
                ("cplex_median_us", "cplex"),
                ("gurobi_median_us", "gurobi"),
                ("mosek_median_us", "mosek"),
            ]:
                vals = [float(e[key]) for e in entries if key in e]
                if vals:
                    series[label].append((s, statistics.median(vals)))

        if any(series.values()):
            for label in ["fast", "cplex", "gurobi", "mosek"]:
                points = sorted(series[label])
                if not points:
                    continue
                sizes = [p[0] for p in points]
                values = [p[1] for p in points]
                ax.plot(sizes, values, marker="o", label=label)
            ax.legend()

        if title:
            ax.set_title(title)
        ax.set_xlabel("S")
        ax.set_ylabel("Median time (us)")
        ax.grid(True, alpha=0.3)

        fig.tight_layout()
        out_path = os.path.join(base_dir, filename)
        fig.savefig(out_path)
        print(f"Wrote {out_path}")


if __name__ == "__main__":
    main()
