#!/usr/bin/env python3

import os
import re
from collections import defaultdict
import math


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


def count_exceptions(path):
    if not os.path.exists(path):
        return 0
    count = 0
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            if line.startswith("exception"):
                count += 1
    return count


def fmt(x):
    if x == 0:
        return "0"
    decimals = max(0, 3 - int(math.floor(math.log10(abs(x)))))
    rounded = round(x, decimals)
    formatted = f"{rounded:,.{decimals}f}"
    if decimals > 0:
        formatted = formatted.rstrip("0").rstrip(".")
    return formatted


def main():
    base_dir = os.path.dirname(__file__)
    runtime_dir = os.path.join(base_dir, "runtime")
    error_dir = os.path.join(base_dir, "errors")

    rows = {}
    errors = {}

    if not os.path.isdir(runtime_dir):
        print("No runtime directory found.")
        return

    for fname in os.listdir(runtime_dir):
        if not fname.endswith(".txt"):
            continue
        path = os.path.join(runtime_dir, fname)
        data = parse_runtime_file(path)
        instance = data.get("instance", "")
        amb = data.get("amb", "")
        key = (instance, amb)
        rows[key] = data
        err_path = os.path.join(error_dir, fname.replace(".txt", ".log"))
        errors[key] = count_exceptions(err_path)

    synthetic = defaultdict(lambda: defaultdict(list))
    benchmarks = defaultdict(list)

    for (instance, amb), data in rows.items():
        if "/synthetic/baseline/" in instance:
            m = re.search(r"S=(\d+)/A=(\d+)/rep=(\d+)", instance)
            if not m:
                continue
            s = int(m.group(1))
            a = int(m.group(2))
            synthetic[(s, a)][amb].append(data)
        else:
            benchmarks[amb].append((instance, data))

    amb_levels = ["mild.amb", "moderate.amb", "severe.amb"]
    print(r"\begin{table}[t]")
    print(r"\centering")
    print(r"\caption{KL Bellman runtimes (all ambiguity levels).}")
    print(r"\begin{tabular}{l|cc|cc|cc}")
    print(r"Instance & \multicolumn{2}{c|}{Mild} & \multicolumn{2}{c|}{Moderate} & \multicolumn{2}{c}{Severe} \\")
    print(r" & Fast (us) & Mosek (us) & Fast (us) & Mosek (us) & Fast (us) & Mosek (us) \\")
    print(r"\hline")

    for (s, a) in sorted(synthetic.keys()):
        label = f"S={s},A={a}"
        cells = []
        for amb in amb_levels:
            entries = synthetic[(s, a)].get(amb, [])
            if entries:
                fast_vals = [float(e["fast_median_us"]) for e in entries]
                mosek_vals = [float(e["mosek_median_us"]) for e in entries]
                fast = sum(fast_vals) / len(fast_vals)
                mosek = sum(mosek_vals) / len(mosek_vals)
                cells.append(f"{fmt(fast)} & {fmt(mosek)}")
            else:
                cells.append("-- & --")
        print(f"{label} & " + " & ".join(cells) + r" \\")

    benchmark_names = sorted({os.path.basename(i) for i, _ in sum(benchmarks.values(), [])})
    for name in benchmark_names:
        cells = []
        for amb in amb_levels:
            entries = [d for (i, d) in benchmarks.get(amb, []) if os.path.basename(i) == name]
            if entries:
                fast = float(entries[0]["fast_median_us"])
                mosek = float(entries[0]["mosek_median_us"])
                cells.append(f"{fmt(fast)} & {fmt(mosek)}")
            else:
                cells.append("-- & --")
        print(f"{name} & " + " & ".join(cells) + r" \\")

    print(r"\end{tabular}")

    notes = []
    for (instance, amb), cnt in sorted(errors.items()):
        if cnt == 0:
            continue
        label = os.path.basename(instance)
        notes.append(f"{label} ({amb}): {cnt} exceptions")

    if notes:
        print(r"\vspace{4pt}")
        print(r"\footnotesize\emph{Notes:} " + "; ".join(notes))

    print(r"\end{table}")


if __name__ == "__main__":
    main()
