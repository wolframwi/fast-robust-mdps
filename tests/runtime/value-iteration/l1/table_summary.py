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


def fmt_optional(value):
    if value is None:
        return "--"
    return fmt(value)


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
    homotopy_dir = os.path.join(base_dir, "runtime_homotopy")

    if not os.path.isdir(runtime_dir):
        print("No runtime directory found.")
        return

    rows = []
    homotopy_rows = {}
    for fname in os.listdir(runtime_dir):
        if not fname.endswith(".txt"):
            continue
        data = parse_runtime_file(os.path.join(runtime_dir, fname))
        if not is_moderate(data):
            continue
        instance = data.get("instance", "")
        rows.append((instance, data))

    if os.path.isdir(homotopy_dir):
        for fname in os.listdir(homotopy_dir):
            if not fname.endswith(".txt"):
                continue
            data = parse_runtime_file(os.path.join(homotopy_dir, fname))
            if not is_moderate(data):
                continue
            instance = data.get("instance", "")
            homotopy_rows[instance] = data

    rows.sort(key=lambda x: x[0])

    print(r"\begin{table}[t]")
    print(r"\centering")
    print(r"\caption{L1 robust value iteration (benchmarks).}")
    print(r"\begin{tabular}{l|cccc|c}")
    print(r"Instance & Fast (s) & CPLEX (s) & Mosek (s) & Homotopy (s) & Iterations \\")
    print(r"\hline")

    notes = []
    for instance, data in rows:
        label = os.path.basename(instance)
        homotopy_data = homotopy_rows.get(instance, {})
        fast_s = float(data.get("fast_vi_s", "0"))
        cplex_s = float(data.get("cplex_vi_s", "0"))
        mosek_s = float(data.get("mosek_vi_s", "0"))
        homotopy_s = (
            float(homotopy_data["homotopy_vi_s"]) if "homotopy_vi_s" in homotopy_data else None
        )
        fast_iters = parse_iteration(data.get("fast_vi_iterations"))
        cplex_iters = parse_iteration(data.get("cplex_vi_iterations"))
        mosek_iters = parse_iteration(data.get("mosek_vi_iterations"))
        homotopy_iters = parse_iteration(homotopy_data.get("homotopy_vi_iterations"))
        iteration_range = fmt_iteration_range(
            [fast_iters, cplex_iters, mosek_iters, homotopy_iters]
        )
        print(
            f"{label} & {fmt(fast_s)} & {fmt(cplex_s)} & {fmt(mosek_s)} & {fmt_optional(homotopy_s)} & "
            f"{iteration_range} \\\\"
        )
        if data.get("cplex_failed", "0") == "1":
            notes.append(f"CPLEX {label}")
        if data.get("mosek_failed", "0") == "1":
            notes.append(f"Mosek {label}")

    print(r"\end{tabular}")

    if notes:
        print(r"\vspace{4pt}")
        print(r"\footnotesize\emph{Notes:} " + ", ".join(notes))

    print(r"\end{table}")


if __name__ == "__main__":
    main()
