#!/usr/bin/env python3
"""Plot TWRF-GTA3 3D Virtual GPU Benchmark Results.

Visualizes frame-by-frame temporal tile reuse, dirty tile ratios,
microarchitectural execution times, and speedup over conventional software rasterization.
"""

from __future__ import annotations

import json
import os
import sys
from typing import Any

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

try:
    import matplotlib.pyplot as plt
    import matplotlib.ticker as ticker
except ImportError:
    print("[ERROR] matplotlib is required to plot benchmark results.")
    sys.exit(1)


def load_benchmark(path: str) -> dict[str, Any]:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def main() -> None:
    results_dir = os.path.join(os.path.dirname(__file__), "..", "..", "results")
    json_path = os.path.join(results_dir, "gta3_twrf_benchmark.json")

    if not os.path.exists(json_path):
        print(f"[ERROR] Benchmark results file not found: {json_path}")
        sys.exit(1)

    data = load_benchmark(json_path)
    frames_data = data.get("frames", [])
    if not frames_data:
        print("[ERROR] No frame records found in benchmark JSON.")
        sys.exit(1)

    frames = [f["frame"] for f in frames_data]
    dirty_tiles = [f["dirty"] for f in frames_data]
    reuse_pct = [f["reuse_ratio"] * 100.0 for f in frames_data]
    twrf_ms = [f["twrf_ms"] for f in frames_data]
    baseline_ms = [f["baseline_ms"] for f in frames_data]
    speedups = [f["speedup"] for f in frames_data]

    avg_reuse = data.get("average_reuse_percent", 0.0)
    cum_speedup = data.get("cumulative_speedup", 1.0)

    # Setup publication-style figure with 2 subplots
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(11, 8), dpi=180, sharex=True)
    plt.subplots_adjust(hspace=0.22)

    # Panel 1: Temporal Tile Reuse & Dirty Count
    color_reuse = "#0284c7"  # Sky blue
    color_dirty = "#ef4444"  # Red

    ax1.plot(frames, reuse_pct, color=color_reuse, linewidth=2.0, label="Temporal Reuse (%)")
    ax1.set_ylabel("Tile Memoization Reuse (%)", color=color_reuse, fontsize=11, fontweight="bold")
    ax1.tick_params(axis="y", labelcolor=color_reuse)
    ax1.set_ylim(0, 105)
    ax1.yaxis.set_major_formatter(ticker.PercentFormatter())
    ax1.grid(True, linestyle="--", alpha=0.5)

    ax1_twin = ax1.twinx()
    ax1_twin.plot(frames, dirty_tiles, color=color_dirty, linewidth=1.5, linestyle="--", label="Dirty Tiles")
    ax1_twin.set_ylabel("Dirty Tiles (Rasterized)", color=color_dirty, fontsize=11, fontweight="bold")
    ax1_twin.tick_params(axis="y", labelcolor=color_dirty)
    ax1_twin.set_ylim(0, max(dirty_tiles) * 1.15)

    ax1.set_title(
        f"TWRF Virtual GPU: 3D Liberty City Workload (960x540, 2040 Tiles)\n"
        f"Mean Temporal Reuse: {avg_reuse:.2f}% | Pure CPU Execution (0% Host GPU)",
        fontsize=12,
        fontweight="bold",
        pad=10
    )

    # Panel 2: Execution Time & Speedup
    color_twrf = "#10b981"      # Emerald green
    color_base = "#64748b"      # Slate grey
    color_speedup = "#8b5cf6"   # Purple

    ax2.plot(frames, twrf_ms, color=color_twrf, linewidth=2.0, label="TWRF Incremental Frame Time (ms)")
    ax2.plot(frames, baseline_ms, color=color_base, linewidth=1.5, linestyle=":", label="Baseline Software Rasterizer (ms)")
    ax2.set_xlabel("Frame Index (Simulation Step)", fontsize=11, fontweight="bold")
    ax2.set_ylabel("Frame Time (ms)", color="#1e293b", fontsize=11, fontweight="bold")
    ax2.set_yscale("log")
    ax2.grid(True, linestyle="--", alpha=0.5)

    ax2_twin = ax2.twinx()
    ax2_twin.plot(frames, speedups, color=color_speedup, linewidth=1.8, label="Instantaneous Speedup Factor")
    ax2_twin.set_ylabel("Speedup Factor (x)", color=color_speedup, fontsize=11, fontweight="bold")
    ax2_twin.tick_params(axis="y", labelcolor=color_speedup)

    # Annotate summary box
    summary_text = (
        f"Benchmark Summary:\n"
        f"- Resolution: 960x540 (16x16 tiles)\n"
        f"- Avg Tile Reuse: {avg_reuse:.1f}%\n"
        f"- Cumulative Speedup: {cum_speedup:.2f}x\n"
        f"- Host GPU Usage: 0.00%"
    )
    ax2.text(
        0.02, 0.25, summary_text,
        transform=ax2.transAxes,
        fontsize=9,
        verticalalignment="top",
        bbox=dict(boxstyle="round,pad=0.5", facecolor="#f8fafc", edgecolor="#cbd5e1", alpha=0.9)
    )

    # Combine legends
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines1_t, labels1_t = ax1_twin.get_legend_handles_labels()
    ax1.legend(lines1 + lines1_t, labels1 + labels1_t, loc="lower right", framealpha=0.9)

    lines2, labels2 = ax2.get_legend_handles_labels()
    lines2_t, labels2_t = ax2_twin.get_legend_handles_labels()
    ax2.legend(lines2 + lines2_t, labels2 + labels2_t, loc="upper right", framealpha=0.9)

    out_plot = os.path.join(results_dir, "gta3_twrf_speedup.png")
    plt.savefig(out_plot, bbox_inches="tight", dpi=180)
    plt.close()

    print("[SUCCESS] Benchmark plot saved to results/gta3_twrf_speedup.png")


if __name__ == "__main__":
    main()
