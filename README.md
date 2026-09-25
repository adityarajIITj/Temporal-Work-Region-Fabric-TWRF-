# Temporal Work Region Fabric (TWRF) with DOOM Virtual GPU Port

Temporal Work Region Fabric (TWRF) is a research virtual-GPU architecture and execution model designed to evaluate persistent, spatially bounded computational work across frames.

This repository branch (`twrf-doom`) integrates id Software's classic **DOOM (1993)** running natively on top of the TWRF virtual GPU architecture. The DOOM port serves as an empirical validation workload for temporal work reuse, demonstrating how spatial-temporal tiling and persistent on-chip state storage yield significant compute cycle reductions and speedups on complex interactive 3D workloads.

---

## 1. Architectural Overview

The core abstraction of the architecture is the **Temporal Work Region (TWR)**: a persistent execution object characterized by a stable identifier, spatial bounding extent, versioned inputs, persistent output state, and explicit dependency tracking.

Mathematically, each region $R_i$ is defined as:

$$R_i = (ID_i, A_i, S_i, I_i, O_i, V_i, \Sigma_i, D_i, Q_i)$$

Where:
- $ID_i$: Unique, persistent spatial identifier.
- $A_i$: Spatial extent bounding box $[x_0, y_0, x_1, y_1]$.
- $S_i$: Execution state ($\text{Clean}$, $\text{Dirty}$, $\text{Executing}$, $\text{Failed}$).
- $I_i$: Versioned input bindings (geometry, textures, camera, uniforms).
- $O_i$: Output buffers mapped to persistent on-chip SRAM.
- $V_i$: Monotonic version tag.
- $\Sigma_i$: Architectural execution signature.
- $D_i$: Explicit dependency list of predecessor TWR IDs.
- $Q_i$: Quality/LOD degradation constraint.

Traditional GPU architectures execute workloads through stateless SIMT thread dispatches. In contrast, TWRF provides hardware-oriented scheduling that treats persistent spatial regions as first-class scheduling entities:

$$\boxed{\text{Persistent Spatial Work Identity} + \text{Persistent State Store} + \text{Version/Change Tracking} + \text{Explicit Dependency Graph} + \text{Hardware Scheduling}}$$

---

## 2. DOOM on TWRF Virtual GPU Architecture

DOOM was ported to TWRF via the standard `doomgeneric` interface coupled with a custom TWRF bridge (`src/doom/doomgeneric_twrf.cpp`).

```
+--------------------------------------------------------------------------+
|                     DOOM Game Engine Core (id Software)                  |
|          Tick -> World Simulation -> BSP Traversal -> Render Frame       |
+--------------------------------------------------------------------------+
                                    |
                        32-bit Framebuffer (640x400)
                                    v
+--------------------------------------------------------------------------+
|                     TWRF Spatial Tiling Engine                           |
|        Decomposes Framebuffer into 16x16 Pixel Temporal Work Regions     |
|                40 Horizontal x 25 Vertical = 1,000 TWR Tiles             |
+--------------------------------------------------------------------------+
                                    |
                    +---------------+---------------+
                    |                               |
       Viewport Region (Rows 0-20)     Status Bar HUD (Rows 21-24)
         Dynamic 3D Geometry              Persistent 2D Overlay
                    |                               |
                    +---------------+---------------+
                                    v
+--------------------------------------------------------------------------+
|                  TWRF Change Tracker & Version Auditor                   |
|        Evaluates input/geometry volatility against previous frame        |
|        Identifies unchanged tiles vs. dirty regions                      |
+--------------------------------------------------------------------------+
             |                                              |
      Dirty / Modified                                Unchanged / Valid
             v                                              v
+--------------------------+               +-------------------------------+
| Execute Tile Kernel      |               | BYPASS RECOMPUTATION (SKIP)   |
| Baseline Rendering Cost  |               | Cost = Zero Compute           |
| Write back to Store      |               | Read Persistent State Output  |
+--------------------------+               +-------------------------------+
             |                                              |
             +----------------------+-----------------------+
                                    v
+--------------------------------------------------------------------------+
|                   TWRF On-Chip Logical State Store                       |
|           1,000 Persistent SRAM Slots (1 MB Capacity Limit)              |
|           Maintains committed frame outputs across boundaries            |
+--------------------------------------------------------------------------+
```

### Spatial-Temporal Decomposition
- **Resolution:** $640 \times 400$ progressive display.
- **Tile Dimension:** $16 \times 16$ pixels per TWR tile.
- **Grid Structure:** $40 \text{ columns} \times 25 \text{ rows} = 1,000 \text{ total TWR tiles}$.
- **Logical State Store:** 1,000 preallocated slots in high-speed persistent on-chip SRAM ($1,024 \text{ bytes per slot} = 1 \text{ MB total capacity}$).

### Dual Temporal Regimes
Interactive game engines exhibit sharp spatial contrast in temporal coherence:
1. **Dynamic 3D Viewport (Rows 0-20):** Rapidly invalidates during player rotation and translation, but preserves significant temporal coherence during forward movement, corridor traversal, and static pauses ($15\% \text{ to } 40\%$ reuse).
2. **Persistent Heads-Up Display (HUD, Rows 21-24):** The bottom $16\%$ of the screen contains the player status bar (ammo, health, arms, armor, and Doomguy status face). Unless health or ammo changes, these 160 tiles remain completely unchanged across successive frames ($94.9\%$ empirical persistence).

---

## 3. Empirical Evaluation & Results

The DOOM benchmark executes id Software's genuine Shareware `doom1.wad` (v1.9) through an automated 150-frame timedemo run (`demo1`, Episode 1 Mission 1: *Hangar*).

### Benchmark Metrics Summary

| Metric | Measured Value | Architectural Significance |
| :--- | :--- | :--- |
| **Simulated Frames** | 150 frames | Multi-frame timedemo loop |
| **Resolution** | $640 \times 400$ | 1,000 TWR tiles per frame |
| **Total Tiles Evaluated** | 150,000 tiles | High statistical sample |
| **Tiles Skipped (Reused)** | 59,677 tiles | **39.78% average tile skip ratio** |
| **Tiles Executed** | 90,323 tiles | Only dynamically dirty tiles rendered |
| **HUD Temporal Persistence** | **94.9%** | Persistent status bar reuse across frames |
| **Baseline SIMT Cycles** | 37,500,000 cycles | Full recompute ($1,000 \times 250 \times 150$) |
| **TWRF Simulated Cycles** | 24,848,750 cycles | Includes change tracking and state store access |
| **Overall Speedup** | **1.51x** | **33.7% net cycle reduction** |

### Frame-by-Frame Behavior

- **Frame 1 (Cold Start):** 1,000 tiles executed (0% reuse). All 1,000 slots allocated and populated in the Logical State Store.
- **Frame 25 (Intro Sequence):** 870 tiles skipped (87.0% temporal reuse). High static background coherence.
- **Frames 50-150 (Active E1M1 Combat & Traversal):** 15% to 25% viewport reuse during motion, with 100% status bar persistence on non-damage frames.

### Graphical Artifacts

- **Architectural Benchmark Plot:** `results/doom_twrf_benchmark.png` (displays frame-by-frame skip ratio and cumulative cycle divergence).
- **Cold Start Snapshot:** `results/doom_frame0_cold.png` (initial frame render).
- **Gameplay Snapshot:** `results/doom_frame25_gameplay.png` (intro sequence transition).
- **Action Snapshot:** `results/doom_frame100_action.png` (E1M1 gameplay state).
- **Machine-Readable Telemetry:** `results/doom_twrf_benchmark.json`.

---

## 4. Formal Cost Model

TWRF formalizes the break-even condition under which temporal reuse outperforms full recomputation:

$$\boxed{C_{\text{TWRF}}(p) = C_t + p \cdot C_r + (1 - p) \cdot C_s}$$

Where:
- $p$: Fraction of modified/dirty regions ($0 \le p \le 1$).
- $C_r$: Full compute cost per region.
- $C_t$: Version tracking and dependency validation overhead per region.
- $C_s$: Persistent State Store read cost per region ($C_s \ll C_r$).

The architecture achieves a performance win whenever:

$$p < 1 - \frac{C_t - C_s}{C_r - C_s} \approx p^*$$

For the DOOM workload with $C_r = 250$ cycles, $C_t = 15.6$ cycles, and $C_s = 0$ cycles, the break-even threshold is $p^* \approx 93.7\%$. Because DOOM maintains an average dirty rate of $p = 60.22\%$ ($39.78\%$ reuse), TWRF operates well within the profitable regime, achieving the measured $1.51\text{x}$ speedup.

---

## 5. Repository Structure

```
.
├── CMakeLists.txt              # Unified build configuration (C++20 & C99/GNU89)
├── README.md                   # Project documentation
├── implementation.md           # DOOM on TWRF architectural design specification
├── run.ps1                     # Automated pipeline runner (Build, Test, Demo, DOOM, Plots)
├── doom1.wad                   # Genuine id Software DOOM Shareware WAD
│
├── doomgeneric/                # DOOM engine C sources (submodule-free)
│   └── doomgeneric/            # Source files (d_main.c, r_draw.c, p_tick.c, etc.)
│
├── include/
│   └── twrf/
│       ├── core/               # TWR types, state store, scheduler, change tracker
│       ├── timing/             # Timing parameters and cycle accounting
│       └── workloads/          # Raster, ray, and neural workload abstractions
│
├── src/
│   ├── core/                   # C++20 core engine implementation
│   ├── doom/
│   │   └── doomgeneric_twrf.cpp # TWRF DOOM bridge, tile grid mapper, benchmark harness
│   └── main.cpp                # Comprehensive TWRF demonstration runner
│
├── tests/                      # 35 rigorous CTest acceptance test suites
├── python/
│   └── analysis/
│       ├── plot_break_even.py  # Scientific break-even curve generator
│       └── plot_doom_results.py # DOOM frame-by-frame analysis and plot generator
│
└── results/                    # Generated experimental data, PNG plots, and JSON sweeps
    ├── doom_twrf_benchmark.json
    ├── doom_twrf_benchmark.png
    ├── doom_frame0_cold.png
    ├── doom_frame25_gameplay.png
    ├── doom_frame100_action.png
    ├── phase3_sweeps.json
    └── break_even_curve.png
```

---

## 6. Build and Verification Instructions

### Prerequisites

- **C++ Compiler:** C++20 compliant compiler (GCC 11+, Clang 13+, or MSVC 2019+). On Windows, MSYS2 UCRT64 GCC 16+ is recommended.
- **C Compiler:** C99 / GNU89 compliant compiler for DOOM sources.
- **Build System:** CMake 3.20+ and Ninja.
- **Python Runtime:** Python 3.9+ with `matplotlib` and `Pillow`.

### Windows Quickstart (PowerShell)

To run the complete pipeline (build, execute all 35 tests, run the core virtual GPU demo, execute the DOOM benchmark, and generate publication plots):

```powershell
.\run.ps1
```

### Manual Build & Execution

#### 1. Configure and Build
```bash
# Configure with CMake and Ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build all targets (libraries, tests, demo, and DOOM)
cmake --build build
```

#### 2. Run Test Suite (35 Acceptance Tests)
```bash
ctest --test-dir build --output-on-failure
```

All 35 registered tests validate:
- Zero false-negative invalidation ($FNI = 0$).
- Strict determinism across scheduling permutations.
- Exact output parity between incremental TWRF execution and full-recompute oracles.
- State store capacity boundaries and eviction policies.

#### 3. Run TWRF Core Simulator Demo
```bash
./build/twrf_demo
```

#### 4. Run TWRF-DOOM Benchmark
```bash
./build/twrf_doom
```
By default, `twrf_doom` executes a 150-frame automated timedemo benchmark over `doom1.wad`, writing telemetry to `results/doom_twrf_benchmark.json` and keyframe snapshots to `results/`.

#### 5. Generate Visualizations and Architectural Plots
```bash
# Generate core break-even curves
python python/analysis/plot_break_even.py

# Generate DOOM frame-by-frame reuse curves and convert frame PPMs
python python/analysis/plot_doom_results.py
```

---

## 7. Continuous Integration

Automated CI is configured via `.github/workflows/validation.yml`, running on `ubuntu-latest`:
- Compiles the full C++20/C codebase with strict warnings.
- Executes all 35 acceptance tests under CTest.
- Executes the core simulation demo and validates JSON schemas.
- Executes the TWRF-DOOM benchmark and verifies performance metrics.
- Uploads experimental artifacts and graphical results.

---

## 8. License & Attribution

- **TWRF Simulator:** Research implementation licensed under the MIT License.
- **DOOM Engine (doomgeneric):** Based on the original id Software DOOM source code released under the GNU General Public License v2 (GPL-2.0).
- **DOOM Shareware WAD:** Copyright (C) 1993-1996 id Software LLC. Included for non-commercial research and evaluation.
