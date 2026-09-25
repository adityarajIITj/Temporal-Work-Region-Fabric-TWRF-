#!/usr/bin/env python3
"""Plot the final TWRF/B1/B2/B3 architectural timing comparison."""

from __future__ import annotations

import json
import os
from typing import Any

import sys
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
import matplotlib.pyplot as plt


def load_results(path: str) -> dict[str, Any]:
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def main() -> None:
    json_path = os.path.join(
        os.path.dirname(__file__), "..", "..", "results", "phase3_sweeps.json"
    )
    if not os.path.exists(json_path):
        print(f"[WARN] Result file not found: {json_path}")
        return

    data = load_results(json_path)
    sweep = data.get("change_rate_sweep", [])
    if not sweep:
        print("[ERROR] No change_rate_sweep data found.")
        return

    p_e = [float(point["executed_region_fraction"]) * 100.0 for point in sweep]
    p_o = [float(point["object_change_fraction"]) * 100.0 for point in sweep]
    twrf = [float(point["twrf_total_cycles"]) for point in sweep]
    base_a = [float(point["baseline_a_total_cycles"]) for point in sweep]
    base_b = [float(point["baseline_b_total_cycles"]) for point in sweep]
    base_c = [float(point["baseline_c_total_cycles"]) for point in sweep]

    print("=" * 96)
    print(
        f"{'p_o (%)':>10} {'p_e (%)':>10} {'TWRF':>16} "
        f"{'Baseline A':>16} {'Baseline B':>16} {'Baseline C':>16}"
    )
    print("-" * 96)
    for po, pe, a, b, c, d in zip(p_o, p_e, twrf, base_a, base_b, base_c):
        print(f"{po:10.1f} {pe:10.1f} {a:16.1f} {b:16.1f} {c:16.1f} {d:16.1f}")
    print("=" * 96)

    plt.figure(figsize=(10, 6), dpi=150)
    plt.plot(p_e, twrf, "o-", linewidth=2.2, label="TWRF")
    plt.plot(p_e, base_a, "--", linewidth=1.8, label="Baseline A: Full recompute")
    plt.plot(p_e, base_b, "-.", linewidth=1.8, label="Baseline B: Temporal cache")
    plt.plot(p_e, base_c, ":", linewidth=2.2, label="Baseline C: Software incremental")

    plt.xlabel("Executed-region fraction p_e (%)")
    plt.ylabel("Derived total execution cycles")
    plt.title("TWRF Architectural Timing Comparison")
    plt.grid(True, linestyle=":", alpha=0.6)
    plt.legend()
    plt.tight_layout()

    out_png = os.path.join(os.path.dirname(json_path), "twrf_architectural_comparison.png")
    plt.savefig(out_png)
    print(f"[INFO] Saved: {out_png}")


if __name__ == "__main__":
    main()
