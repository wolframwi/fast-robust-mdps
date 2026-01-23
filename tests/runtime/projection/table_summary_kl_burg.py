#!/usr/bin/env python3

import os
import re
import statistics


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


def is_moderate(data):
    amb = data.get("amb", "")
    if amb and "moderate" not in amb:
        return False
    return True


def parse_float(value):
    if value is None:
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def fmt(value):
    if value is None:
        return "--"
    return f"{value:,.2f}"


def parse_sa_rep(instance):
    s_match = re.search(r"S=(\d+)", instance)
    a_match = re.search(r"A=(\d+)", instance)
    rep_match = re.search(r"rep=(\d+)", instance)
    s_val = int(s_match.group(1)) if s_match else None
    a_val = int(a_match.group(1)) if a_match else None
    rep_val = int(rep_match.group(1)) if rep_match else None
    return s_val, a_val, rep_val


def is_synthetic_instance(instance):
    return "/synthetic/" in instance or instance.startswith("synthetic::")


def synthetic_key(instance):
    s_val, a_val, _ = parse_sa_rep(instance)
    if s_val is None or a_val is None:
        return None
    parts = instance.split("/synthetic/", 1)
    if len(parts) != 2:
        return None
    suffix = parts[1]
    prefix = suffix.split("/S=", 1)[0]
    return f"synthetic::{prefix}::S={s_val}::A={a_val}"


def load_rows(runtime_dir):
    rows = {}
    if not os.path.isdir(runtime_dir):
        return rows
    for fname in os.listdir(runtime_dir):
        if not fname.endswith(".txt"):
            continue
        data = parse_runtime_file(os.path.join(runtime_dir, fname))
        if not is_moderate(data):
            continue
        instance = data.get("instance", "")
        if not instance:
            continue
        if is_synthetic_instance(instance):
            key = synthetic_key(instance)
        else:
            key = instance
        if key:
            rows.setdefault(key, []).append((instance, data))
    return rows


def instance_group(instance):
    s_val, a_val, _ = parse_sa_rep(instance)
    if s_val is not None and a_val == 10:
        return 0
    if s_val is not None and a_val is not None and s_val == a_val:
        return 1
    return 2


def instance_sort_key(instance):
    group = instance_group(instance)
    s_val, a_val, rep_val = parse_sa_rep(instance)
    if s_val is not None and a_val is not None:
        rep_val = rep_val if rep_val is not None else -1
        return (group, s_val, a_val, rep_val, instance)
    return (group, instance)


def main():
    base_dir = os.path.dirname(__file__)
    kl_runtime_dir = os.path.join(base_dir, "kl", "runtime")
    burg_runtime_dir = os.path.join(base_dir, "burg", "runtime")

    kl_rows = load_rows(kl_runtime_dir)
    burg_rows = load_rows(burg_runtime_dir)

    instances = sorted(set(kl_rows.keys()) | set(burg_rows.keys()), key=instance_sort_key)

    print(r"\begin{table}[t]")
    print(r"\centering")
    print(r"\caption{KL vs.\ Burg projection runtimes (benchmarks, $\mu$s).}")
    print(r"\begin{tabular}{l|cc|cc}")
    print(r"Instance & KL Fast & KL Mosek & Burg Fast & Burg Mosek \\")
    print(r"\hline")

    last_group = None
    for instance in instances:
        group = instance_group(instance)
        if last_group is not None and group != last_group:
            print(r"\hline")
        last_group = group
        if is_synthetic_instance(instance):
            s_val, a_val, _ = parse_sa_rep(instance)
            label = f"synthetic ($S={s_val}$, $A={a_val}$)"
        else:
            label = os.path.basename(instance)

        kl_entries = kl_rows.get(instance, [])
        burg_entries = burg_rows.get(instance, [])

        def median_value(entries, key):
            values = [parse_float(data.get(key)) for _, data in entries]
            values = [v for v in values if v is not None]
            if not values:
                return None
            return statistics.median(values)

        kl_fast = median_value(kl_entries, "fast_median_us")
        kl_mosek = median_value(kl_entries, "mosek_median_us")
        burg_fast = median_value(burg_entries, "fast_median_us")
        burg_mosek = median_value(burg_entries, "mosek_median_us")

        print(
            f"{label} & {fmt(kl_fast)} & {fmt(kl_mosek)} & "
            f"{fmt(burg_fast)} & {fmt(burg_mosek)} \\\\"
        )

    print(r"\end{tabular}")
    print(r"\end{table}")


if __name__ == "__main__":
    main()
