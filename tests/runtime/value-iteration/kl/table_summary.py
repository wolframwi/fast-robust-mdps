#!/usr/bin/env python3

import os
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


def fmt(x):
    if x == 0:
        return "0"
    decimals = max(0, 3 - int(math.floor(math.log10(abs(x)))))
    rounded = round(x, decimals)
    formatted = f"{rounded:,.{decimals}f}"
    if decimals > 0:
        formatted = formatted.rstrip("0").rstrip(".")
    return formatted


def parse_iteration(value):
    if value is None:
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def fmt_iteration_range(values):
    iterations = [v for v in values if v is not None]
    if not iterations:
        return "--"
    return f"{min(iterations)}--{max(iterations)}"


def is_moderate(data):
    amb = data.get("amb", "")
    if amb and "moderate" not in amb:
        return False
    return True


def main():
    base_dir = os.path.dirname(__file__)
    runtime_dir = os.path.join(base_dir, "runtime")

    if not os.path.isdir(runtime_dir):
        print("No runtime directory found.")
        return

    rows = []
    for fname in os.listdir(runtime_dir):
        if not fname.endswith(".txt"):
            continue
        data = parse_runtime_file(os.path.join(runtime_dir, fname))
        if not is_moderate(data):
            continue
        instance = data.get("instance", "")
        rows.append((instance, data))

    rows.sort(key=lambda x: x[0])

    print(r"\begin{table}[t]")
    print(r"\centering")
    print(r"\caption{KL robust value iteration (benchmarks).}")
    print(r"\begin{tabular}{l|cc|c}")
    print(r"Instance & Fast (s) & Mosek (s) & Iterations \\")
    print(r"\hline")

    notes = []
    for instance, data in rows:
        label = os.path.basename(instance)
        fast_s = float(data.get("fast_vi_s", "0"))
        mosek_s = float(data.get("mosek_vi_s", "0"))
        fast_iters = parse_iteration(data.get("fast_vi_iterations"))
        mosek_iters = parse_iteration(data.get("mosek_vi_iterations"))
        iteration_range = fmt_iteration_range([fast_iters, mosek_iters])
        print(
            f"{label} & {fmt(fast_s)} & {fmt(mosek_s)} & {iteration_range} \\\\"
        )
        if data.get("mosek_failed", "0") == "1":
            notes.append(label)

    print(r"\end{tabular}")

    if notes:
        print(r"\vspace{4pt}")
        print(r"\footnotesize\emph{Notes:} MOSEK failed on " + ", ".join(notes))

    print(r"\end{table}")


if __name__ == "__main__":
    main()
