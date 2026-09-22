# Temporal Work Region Fabric (TWRF)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-green.svg)](https://cmake.org/)
[![Tests](https://img.shields.io/badge/Tests-26%20Passing%20(100%25)-brightgreen.svg)](tests/)
[![Status](https://img.shields.io/badge/Architecture-Phases%201--4%20Complete-success.svg)](TWRF_4_SubPlans_Coding_Agent.md)
[![License](https://img.shields.io/badge/License-MIT-lightgrey.svg)](LICENSE)

> **In a nutshell:** TWRF is a virtual GPU architecture simulator that skips recomputing unchanged screen regions across frames. Instead of recalculating every pixel from scratch 60–120 times a second, TWRF stores results in persistent on-chip memory and re-executes only what actually changed.

---

## The Big Idea

### The Problem: Unconditional Recomputation
Standard modern GPUs (NVIDIA, AMD, Apple) are built on an unconditional model: **every single frame, the entire screen is recomputed from scratch**. Even if a player stands still in a game and 90% of the screen doesn't move, the GPU throws away the previous frame's work and recalculates the exact same pixels again. This wastes massive amounts of power and compute cycles.

### The TWRF Solution: Persistent Temporal Dataflow
TWRF introduces the **Temporal Work Region (TWR)**:
1. The screen and computation stages are divided into bounded spatial units (screen tiles, ray bundles, AI denoising blocks).
2. Each unit saves its output in a fast, on-chip **Persistent State Store**.
3. Each unit tracks its inputs (camera, meshes, textures, lights) using lightweight integer version numbers.
4. **If inputs haven't changed $\implies$ skip computation completely and reuse the cached result.**
5. **If inputs changed $\implies$ recompute only that unit and pass the new result downstream.**

---

## Key Findings & Results (At a Glance)

We tested TWRF across a full spectrum of scene movement—from completely still scenes ($p = 0\%$) to violent camera motion ($p = 100\%$):

| Scene Movement ($p$) | What Happens | TWRF Speedup / Tax | Status |
| :---: | :--- | :---: | :---: |
| **0% (Still scene)** | 100% of tiles skipped | **1.83x Speedup** (8.7k vs 15.9k cycles) | **Big WIN** |
| **5% – 10% (Subtle motion)** | ~94% of tiles skipped | **1.10x – 1.15x Speedup** | **WIN** |
| **~13% (Break-even)** | Tracking overhead balances recomputation | **Equal performance ($p^* \approx 13\%$)** | **Break-Even** |
| **25% – 75% (Fast motion)** | Most tiles recomputed | **+27% to +96% cycle penalty** | **Loss** |
| **100% (Full screen change)** | All tiles recomputed + tracking cost paid | **+176.4% cycle tax** (44.1k vs 15.9k cycles) | **Worst-Case** |

> [!NOTE]
> **Takeaway:** TWRF delivers major speedups on static or moderately dynamic scenes ($p \le 10\%$). For high-action scenes ($p > 13\%$), the tracking overhead outweighs the benefit, requiring an automatic fallback switch to standard full recomputation.

---

## System Architecture in 3 Steps

```
[ Scene Inputs ] ----> [ Change Tracker ] ----> [ Ready Queue ] ----> [ Compute Units ] ----> [ Persistent State Store ]
 (Camera/Objects)       (Did inputs bump?)       (Priority Order)      (Raster/Ray/Neural)      (Cached Tile Memory)
```

1. **Change Tracking ($O(1)$ scalar checks):** Inputs are stamped with version numbers. If a mesh moves, its version increments, and only the screen tiles overlapping its bounding box are flagged dirty.
2. **Unified Heterogeneous DAG:** TWRF uses the **exact same execution model** for 3D rasterization, ray tracing, and neural AI reconstruction:
   $$\text{Raster G-Buffer} \longrightarrow \text{Ray Traced Shadows} \longrightarrow \text{Neural Denoising MLP}$$
   *If a light moves in the scene, the raster stage is automatically skipped, while only the shadow rays and neural denoiser re-run!*
3. **Deterministic Ready Queue:** Work is ordered with strict priority and topological depth tie-breaking, ensuring 100% bitwise-reproducible frames.

---

## Quickstart (Run in 60 Seconds)

### One-Click Runner (Windows PowerShell)
```powershell
.\run.ps1
```
This automatically compiles the simulator, executes all 26 verification tests, runs the 3D demo, and plots the break-even curve.

### Manual Build (Windows / Linux / macOS)
```bash
# 1. Configure and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 2. Run all 26 automated unit & regression tests
ctest --test-dir build --output-on-failure

# 3. Run the interactive 3D virtual GPU demo
./build/twrf_demo

# 4. Generate the break-even plot
python python/analysis/plot_break_even.py
```

Generated outputs will appear in the `results/` folder:
- `results/demo_frame0.ppm` — Cold start frame (all tiles rendered).
- `results/demo_frame1_static.ppm` — Static frame (100% temporal reuse, 0 tiles recomputed).
- `results/demo_frame2_dynamic.ppm` — Moving object frame (only dirty tiles recomputed).
- `results/break_even_curve.png` — Graphical benchmark plot.

---

## Documentation Hub

Looking for deep technical details, mathematical proofs, or developer APIs? Explore the specialized documentation:

| Guide | Description |
| :--- | :--- |
| **[Technical Manual & Methodology](docs/TECHNICAL_MANUAL.md)** | Full technicalities: mathematical cost model proofs, scheduling algorithms, memory architectures, and detailed cycle profiling. |
| **[C++ API Reference](docs/API_REFERENCE.md)** | Complete developer guide: classes, methods, structs, invariants, and code examples for all namespaces (`core`, `raster`, `timing`, `ray`, `neural`, `api`). |
| **[Architecture Specification v0.4](docs/TWRF_SPEC.md)** | Formal architecture specification, execution primitives, state machine invariants, and deviations log. |
| **[Limitations & Failure Regimes](docs/LIMITATIONS.md)** | Transparent analysis of pathological workloads (rapid camera pan, rain particles, divergent rays) and hardware fallback policies. |
| **[Prior-Art & Novelty Taxonomy](docs/PRIOR_ART.md)** | Honest academic comparison separating established prior art (TBDR, TAA, DLSS, SIMT) from TWRF's architectural combinations. |
| **[FPGA & RTL Synthesizability](docs/FPGA_FEASIBILITY.md)** | Proposed hardware mapping on AMD Xilinx UltraScale+ ZCU102 (~12.1% LUT utilization) and SystemVerilog testbench strategy. |
| **[Architecture Decision Records](docs/ARCHITECTURE_DECISIONS.md)** | ADR-001 through ADR-007 documenting key architectural choices (version vectors vs hashes, logical state store, dynamic pruning). |

---

## Test Suite Status

All **26 acceptance tests** pass with 100% determinism:

```
Test project build/
      Start  1: test_twr_lifecycle ...............   Passed    (P1: Lifecycle & State Transitions)
      Start  2: test_change_detection ............   Passed    (P1: Versioning & Bounding Filters)
      Start  3: test_dependency_propagation ......   Passed    (P1: Upstream Producer Invalidation)
      Start  4: test_scheduler_determinism .......   Passed    (P1: ReadyQueue Tie-Breaking)
      Start  5: test_cost_model ..................   Passed    (P1: Break-Even Correctness)
      Start  6: test_state_store .................   Passed    (P1: Dual-Port SRAM Allocation)
      Start  7: test_multi_frame .................   Passed    (P1: Multi-Frame Persistence)
      Start  8: test_raster_reuse ................   Passed    (P2: 100% Static Tile Reuse)
      Start  9: test_object_transform ............   Passed    (P2: Localized Mesh Translation)
      Start 10: test_camera_change ...............   Passed    (P2: Camera Motion Parity)
      Start 11: test_texture_update ..............   Passed    (P2: Texture Modification)
      Start 12: test_dynamic_scene ...............   Passed    (P2: Bitwise Oracle Equivalence)
      Start 13: test_tile_sweep ..................   Passed    (P2: 8x8, 16x16, 32x32 Tiles)
      Start 14: test_raster_repeatability ........   Passed    (P2: Bitwise Repeatability)
      Start 15: test_model_accounting ............   Passed    (P3: 5-Component Cycle Attribution)
      Start 16: test_baseline_parity .............   Passed    (P3: SIMT Baseline Equivalence)
      Start 17: test_change_rate_sweep ...........   Passed    (P3: Volatility Sweep Verification)
      Start 18: test_break_even_sanity ...........   Passed    (P3: Break-Even Boundary Check)
      Start 19: test_worst_case ..................   Passed    (P3: 100% Dynamic Regime Tolerance)
      Start 20: test_sweep_reproducibility .......   Passed    (P3: Cycle Trace Reproducibility)
      Start 21: test_ray_batch_reuse .............   Passed    (P4: Ray Batch Reuse & Invalidation)
      Start 22: test_neural_inference ............   Passed    (P4: Neural Denoising Determinism)
      Start 23: test_temporal_neural_state .......   Passed    (P4: Recurrent Hidden State)
      Start 24: test_cross_workload_dag ..........   Passed    (P4: Cross-Stage Raster->Ray->Neural)
      Start 25: test_heterogeneous_fallback ......   Passed    (P4: Forced Recomputation Oracle)
      Start 26: test_api_replay ..................   Passed    (P4: Native API Command Replay)

100% tests passed out of 26 (Total time: ~0.5s)
```

---

## License

This research simulator is released under the [MIT License](LICENSE).
