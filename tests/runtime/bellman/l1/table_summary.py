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
    homotopy_dir = os.path.join(base_dir, "runtime_homotopy")
    homotopy_error_dir = os.path.join(base_dir, "errors_homotopy")

    rows = {}
    errors = {}
    homotopy_rows = {}
    homotopy_errors = {}

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

    if os.path.isdir(homotopy_dir):
        for fname in os.listdir(homotopy_dir):
            if not fname.endswith(".txt"):
                continue
            path = os.path.join(homotopy_dir, fname)
            data = parse_runtime_file(path)
            instance = data.get("instance", "")
            amb = data.get("amb", "")
            key = (instance, amb)
            homotopy_rows[key] = data
            err_path = os.path.join(homotopy_error_dir, fname.replace(".txt", ".log"))
            homotopy_errors[key] = count_exceptions(err_path)

    synthetic = defaultdict(list)
    benchmarks = []
    homotopy_synthetic = defaultdict(list)
    homotopy_benchmarks = []
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

    for (instance, amb), data in homotopy_rows.items():
        if amb != "moderate.amb":
            continue
        if "/synthetic/baseline/" in instance:
            m = re.search(r"S=(\d+)/A=(\d+)/rep=(\d+)", instance)
            if not m:
                continue
            s = int(m.group(1))
            a = int(m.group(2))
            homotopy_synthetic[(s, a)].append(data)
        else:
            homotopy_benchmarks.append((instance, data))

    print(r"\begin{table}[t]")
    print(r"\centering")
    print(r"\caption{L1 Bellman runtimes (moderate ambiguity).}")
    print(r"\begin{tabular}{l|cccc}")
    print(r"Instance & Fast (us) & CPLEX (us) & Mosek (us) & Homotopy (us) \\")
    print(r"\hline")

    synthetic_keys = set(synthetic.keys()) | set(homotopy_synthetic.keys())
    for (s, a) in sorted(synthetic_keys):
        entries = synthetic.get((s, a), [])
        homotopy_entries = homotopy_synthetic.get((s, a), [])
        fast_vals = [float(e["fast_median_us"]) for e in entries if "fast_median_us" in e]
        cplex_vals = [float(e["cplex_median_us"]) for e in entries if "cplex_median_us" in e]
        mosek_vals = [float(e["mosek_median_us"]) for e in entries if "mosek_median_us" in e]
        homotopy_vals = [
            float(e["homotopy_median_us"])
            for e in homotopy_entries
            if "homotopy_median_us" in e
        ]
        fast = sum(fast_vals) / len(fast_vals) if fast_vals else None
        cplex = sum(cplex_vals) / len(cplex_vals) if cplex_vals else None
        mosek = sum(mosek_vals) / len(mosek_vals) if mosek_vals else None
        homotopy = sum(homotopy_vals) / len(homotopy_vals) if homotopy_vals else None
        label = f"S={s},A={a}"
        print(
            f"{label} & {fmt(fast) if fast is not None else '--'} & "
            f"{fmt(cplex) if cplex is not None else '--'} & "
            f"{fmt(mosek) if mosek is not None else '--'} & "
            f"{fmt(homotopy) if homotopy is not None else '--'} \\\\"
        )

    benchmark_instances = {i for (i, _) in benchmarks} | {i for (i, _) in homotopy_benchmarks}
    for instance in sorted(benchmark_instances):
        data = next((d for (i, d) in benchmarks if i == instance), {})
        label = os.path.basename(instance)
        homotopy_data = next((d for (i, d) in homotopy_benchmarks if i == instance), {})
        fast = float(data["fast_median_us"]) if "fast_median_us" in data else None
        cplex = float(data["cplex_median_us"]) if "cplex_median_us" in data else None
        mosek = float(data["mosek_median_us"]) if "mosek_median_us" in data else None
        homotopy = (
            float(homotopy_data["homotopy_median_us"])
            if "homotopy_median_us" in homotopy_data
            else None
        )
        print(
            f"{label} & {fmt(fast) if fast is not None else '--'} & "
            f"{fmt(cplex) if cplex is not None else '--'} & "
            f"{fmt(mosek) if mosek is not None else '--'} & "
            f"{fmt(homotopy) if homotopy is not None else '--'} \\\\"
        )

    print(r"\end{tabular}")

    notes = []
    for (instance, amb), cnt in sorted(errors.items()):
        if amb != "moderate.amb" or cnt == 0:
            continue
        label = os.path.basename(instance)
        notes.append(f"{label}: {cnt} exceptions")

    for (instance, amb), cnt in sorted(homotopy_errors.items()):
        if amb != "moderate.amb" or cnt == 0:
            continue
        label = os.path.basename(instance)
        notes.append(f"{label}: {cnt} homotopy exceptions")

    if notes:
        print(r"\vspace{4pt}")
        print(r"\footnotesize\emph{Notes:} " + "; ".join(notes))

    print(r"\end{table}")


if __name__ == "__main__":
    main()
