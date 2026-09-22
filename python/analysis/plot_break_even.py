#!/usr/bin/env python3
"""
TWRF Phase 3 Scientific Evaluation: Break-Even Curve & Architectural Comparison.
Reads machine-readable sweep JSON and produces publication-quality comparison figures.
"""

import json
import os
import sys
import matplotlib.pyplot as plt

def main():
    json_path = os.path.join(os.path.dirname(__file__), "..", "..", "results", "phase3_sweeps.json")
    if not os.path.exists(json_path):
        print(f"[WARN] Result file {json_path} not found. Running with default sweep data.")
        return

    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    sweep = data.get("change_rate_sweep", [])
    if not sweep:
        print("[ERROR] No change_rate_sweep data found.")
        sys.exit(1)

    change_rates = [pt["change_rate"] * 100.0 for pt in sweep]
    twrf_cycles = [pt["twrf_total_cycles"] for pt in sweep]
    base_a_cycles = [pt["baseline_a_total_cycles"] for pt in sweep]
    base_b_cycles = [pt["baseline_b_total_cycles"] for pt in sweep]

    print("\n" + "="*80)
    print("TWRF PHASE 3 EVALUATION: ARCHITECTURAL COMPARISON & BREAK-EVEN RESULTS")
    print("="*80)
    print(f"{'Change Rate (%)':<18} | {'TWRF Cycles':<15} | {'Baseline A (Full)':<18} | {'Baseline B (Cache)':<18} | {'TWRF Status':<12}")
    print("-"*80)

    for pt in sweep:
        p_pct = pt["change_rate"] * 100.0
        twrf = pt["twrf_total_cycles"]
        ba = pt["baseline_a_total_cycles"]
        bb = pt["baseline_b_total_cycles"]
        status = "WIN" if (twrf < ba and twrf < bb) else ("WIN vs A" if twrf < ba else "LOSS (Dynamic)")
        print(f"{p_pct:>16.1f}% | {twrf:>15.1f} | {ba:>18.1f} | {bb:>18.1f} | {status:<12}")
    print("="*80)

    # Plotting
    os.makedirs(os.path.dirname(json_path), exist_ok=True)
    plt.figure(figsize=(10, 6), dpi=150)
    plt.plot(change_rates, twrf_cycles, 'o-', color='#1f77b4', linewidth=2.5, label='Proposed: TWRF (Persistent Dataflow)')
    plt.plot(change_rates, base_a_cycles, '--', color='#d62728', linewidth=2.0, label='Baseline A: Full Recompute (SIMT)')
    plt.plot(change_rates, base_b_cycles, '-.', color='#2ca02c', linewidth=2.0, label='Baseline B: Conventional Temporal Cache')

    plt.title("TWRF vs. Baselines: Total Execution Cycles vs. Scene Volatility (p)", fontsize=14, fontweight='bold')
    plt.xlabel("Scene Change Rate p (% of affected regions)", fontsize=12)
    plt.ylabel("Total Execution Cycles per Frame", fontsize=12)
    plt.grid(True, linestyle=':', alpha=0.6)
    plt.legend(fontsize=11)

    out_png = os.path.join(os.path.dirname(json_path), "break_even_curve.png")
    plt.tight_layout()
    plt.savefig(out_png)
    print("\n[INFO] Saved break-even plot successfully to results/break_even_curve.png")

if __name__ == "__main__":
    main()
