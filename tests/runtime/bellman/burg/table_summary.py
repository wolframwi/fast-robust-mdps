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

    synthetic = defaultdict(list)
    benchmarks = []
    for (instance, amb), data in rows.items():
        if amb != "moderate.amb":
            continue
        if "/synthetic/baseline/" in instance:
            m = re.search(r"S=(\d+)/A=(\d+)/rep=(\d+)", instance)
            if not m:
                continue
            s = int(m.group(1))
            a = int(m.group(2))
            synthetic[(s, a)].append(data)
        else:
            benchmarks.append((instance, data))

    print(r"\begin{table}[t]")
    print(r"\centering")
    print(r"\caption{Burg Bellman runtimes (moderate ambiguity).}")
    print(r"\begin{tabular}{l|cc}")
    print(r"Instance & Fast (us) & Mosek (us) \\")
    print(r"\hline")

    for (s, a) in sorted(synthetic.keys()):
        entries = synthetic[(s, a)]
        fast_vals = [float(e["fast_median_us"]) for e in entries]
        mosek_vals = [float(e["mosek_median_us"]) for e in entries]
        fast = sum(fast_vals) / len(fast_vals)
        mosek = sum(mosek_vals) / len(mosek_vals)
        label = f"S={s},A={a}"
        print(f"{label} & {fmt(fast)} & {fmt(mosek)} \\\\")

    for instance, data in sorted(benchmarks, key=lambda x: x[0]):
        label = os.path.basename(instance)
        fast = float(data["fast_median_us"])
        mosek = float(data["mosek_median_us"])
        print(f"{label} & {fmt(fast)} & {fmt(mosek)} \\\\")

    print(r"\end{tabular}")

    notes = []
    for (instance, amb), cnt in sorted(errors.items()):
        if amb != "moderate.amb" or cnt == 0:
            continue
        label = os.path.basename(instance)
        notes.append(f"{label}: {cnt} exceptions")

    if notes:
        print(r"\vspace{4pt}")
        print(r"\footnotesize\emph{Notes:} " + "; ".join(notes))

    print(r"\end{table}")


if __name__ == "__main__":
    main()
