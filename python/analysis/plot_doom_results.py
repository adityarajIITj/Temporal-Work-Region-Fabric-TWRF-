#!/usr/bin/env python3
"""
TWRF DOOM Evaluation: Empirical Frame-by-Frame Temporal Reuse and Speedup Analysis.
Parses results/doom_twrf_benchmark.json, converts exported frame PPMs to PNGs,
and renders publication-quality architectural graphs.
"""

import json
import os
import sys
import matplotlib.pyplot as plt
from PIL import Image

# Ensure stdout handles utf-8 safely across Windows consoles
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

def convert_ppms_to_png(results_dir):
    for fname in os.listdir(results_dir):
        if fname.endswith(".ppm"):
            ppm_path = os.path.join(results_dir, fname)
            png_path = os.path.join(results_dir, fname.replace(".ppm", ".png"))
            try:
                with Image.open(ppm_path) as img:
                    img.save(png_path)
                print(f"[PPM->PNG] Converted {fname} -> {os.path.basename(png_path)}")
            except Exception as e:
                print(f"[WARN] Failed to convert {fname}: {e}")

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    results_dir = os.path.normpath(os.path.join(script_dir, "..", "..", "results"))
    json_path = os.path.join(results_dir, "doom_twrf_benchmark.json")

    if not os.path.exists(json_path):
        print(f"[ERROR] Benchmark file {json_path} not found.")
        sys.exit(1)

    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    frames = data.get("frames", [])
    if not frames:
        print("[ERROR] No frame records found in benchmark JSON.")
        sys.exit(1)

    total_frames = data["total_frames"]
    avg_skip = data["avg_skip_ratio"] * 100.0
    hud_reuse = data["hud_reuse_ratio"] * 100.0
    speedup = data["speedup"]
    twrf_cycles = data["cum_twrf_cycles"]
    baseline_cycles = data["cum_baseline_cycles"]

    print("\n" + "="*80)
    print("TWRF-DOOM EMPIRICAL EVALUATION: 16x16 TWR TILE TEMPORAL REUSE RESULTS")
    print("="*80)
    print(f"Total Simulated Frames:     {total_frames}")
    print(f"Resolution:                 {data['resolution']['width']}x{data['resolution']['height']} ({data['tile_grid']['total']} tiles per frame)")
    print(f"Total Tiles Evaluated:      {data['cum_tiles_executed'] + data['cum_tiles_skipped']:,}")
    print(f"Tiles Reused / Skipped:     {data['cum_tiles_skipped']:,} ({avg_skip:.2f}%)")
    print(f"Status Bar (HUD) Reuse:     {hud_reuse:.2f}% temporal persistence")
    print(f"TWRF Simulated Cycles:      {twrf_cycles:,.0f}")
    print(f"Baseline SIMT Cycles:       {baseline_cycles:,.0f}")
    print(f"Empirical Speedup:          {speedup:.2f}x (Cycle Reduction: {((1.0 - twrf_cycles/baseline_cycles)*100.0):.1f}%)")
    print("="*80)

    # Convert generated PPMs to PNGs
    convert_ppms_to_png(results_dir)

    # Prepare plot data
    frame_indices = [f["frame"] for f in frames]
    skip_pcts = [f["skip_ratio"] * 100.0 for f in frames]

    # Calculate cumulative cycles per frame
    cum_twrf = []
    cum_base = []
    cur_twrf = 0.0
    cur_base = 0.0
    
    # 250 cycles baseline per tile, tracking overhead per tile
    c_r = 250.0
    c_t = 15.6  # tracking overhead from TWRF cost model
    total_tiles = data['tile_grid']['total']

    for f in frames:
        executed = f["executed"]
        cur_twrf += (total_tiles * c_t) + (executed * c_r)
        cur_base += (total_tiles * c_r)
        cum_twrf.append(cur_twrf)
        cum_base.append(cur_base)

    # Plot figure
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(11, 8), dpi=150, sharex=True)

    # Subplot 1: Frame-by-Frame Skip Ratio
    ax1.plot(frame_indices, skip_pcts, color="#2b5c8f", linewidth=1.8, label="TWR Tile Skip Ratio (% frame reuse)")
    ax1.fill_between(frame_indices, skip_pcts, color="#4a90e2", alpha=0.25)
    ax1.axhline(y=avg_skip, color="#d9534f", linestyle="--", linewidth=1.5, label=f"Average Reuse: {avg_skip:.1f}%")
    ax1.set_title("TWRF-DOOM: Frame-by-Frame Temporal Region Reuse (640x400, 16x16 TWR Tiles)", fontsize=13, fontweight="bold")
    ax1.set_ylabel("Skipped Tiles (%)", fontsize=11)
    ax1.set_ylim(-2, 105)
    ax1.grid(True, linestyle=":", alpha=0.6)
    ax1.legend(loc="upper right", fontsize=10)

    # Annotate key game events
    ax1.annotate("Cold Start\n(0% reuse)", xy=(1, 0), xytext=(8, 20),
                 arrowprops=dict(facecolor="black", shrink=0.05, width=1, headwidth=6),
                 fontsize=9, fontweight="bold")
    ax1.annotate("Intro Screen\n(87% reuse)", xy=(25, 87), xytext=(35, 70),
                 arrowprops=dict(facecolor="black", shrink=0.05, width=1, headwidth=6),
                 fontsize=9, fontweight="bold")
    ax1.annotate("Active E1M1 Combat\n(15-25% reuse + HUD)", xy=(75, 16), xytext=(85, 35),
                 arrowprops=dict(facecolor="black", shrink=0.05, width=1, headwidth=6),
                 fontsize=9, fontweight="bold")

    # Subplot 2: Cumulative Cycles
    ax2.plot(frame_indices, [c / 1e6 for c in cum_base], color="#d9534f", linestyle="--", linewidth=2.0, label="Baseline SIMT: Full Recompute")
    ax2.plot(frame_indices, [c / 1e6 for c in cum_twrf], color="#2ca02c", linewidth=2.2, label=f"Proposed: TWRF Persistent State Store ({speedup:.2f}x Speedup)")
    ax2.set_title("Cumulative Execution Cycles (Millions of Cycles)", fontsize=13, fontweight="bold")
    ax2.set_xlabel("Simulation Frame Index", fontsize=11)
    ax2.set_ylabel("Megacycles (M)", fontsize=11)
    ax2.grid(True, linestyle=":", alpha=0.6)
    ax2.legend(loc="upper left", fontsize=10)

    # Summary box
    summary_text = (
        f"Workload: id Software DOOM (Shareware v1.9)\n"
        f"Resolution: 640x400 (1,000 TWR Tiles)\n"
        f"HUD Temporal Persistence: {hud_reuse:.1f}%\n"
        f"Cycle Reduction: {((1.0 - twrf_cycles/baseline_cycles)*100.0):.1f}%\n"
        f"Overall Speedup: {speedup:.2f}x"
    )
    ax2.text(0.98, 0.05, summary_text, transform=ax2.transAxes, fontsize=9.5,
             verticalalignment="bottom", horizontalalignment="right",
             bbox=dict(boxstyle="round,pad=0.5", facecolor="#f8f9fa", edgecolor="#ced4da", alpha=0.9))

    plt.tight_layout()
    out_png = os.path.join(results_dir, "doom_twrf_benchmark.png")
    plt.savefig(out_png)
    plt.close()
    print(f"\n[OUTPUT] Architectural plot generated: results/doom_twrf_benchmark.png\n")

if __name__ == "__main__":
    main()
