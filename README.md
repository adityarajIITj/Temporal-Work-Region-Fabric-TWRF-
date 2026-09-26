# TWRF-DOOM

Running Classic DOOM on an experimental virtual GPU that only renders what changes.

---

## The Big Idea

Modern GPUs are brute-force machines. In most real-time games, large chunks of the screen look identical from one frame to the next, yet conventional hardware still recalculates every single pixel from scratch 60 or 120 times every second.

**Temporal Work Region Fabric (TWRF)** is an experimental virtual GPU architecture built to test a different approach: what happens if the GPU treats spatial regions as persistent, stateful objects across time? Instead of redrawing the entire display, TWRF tracks changes between frames and completely bypasses rendering for anything that stayed the same.

To stress-test this architecture on real interactive 3D gameplay, we ported id Software's legendary **DOOM (1993)** to run directly on top of the TWRF virtual GPU simulator.

---

## Why DOOM?

DOOM is the classic proving ground for graphics architectures, but it also highlights real-world rendering redundancy:

1. **The Status Bar (HUD):** The bottom strip of the screen displays your health, ammo, weapons, and armor. In any given firefight, these pixels rarely change from frame to frame.
2. **Corridors and Ceilings:** While walking forward down a hall or waiting in ambush, large blocks of textures and geometry remain static.
3. **Action Bursts:** When turning corners or firing shotguns, localized regions update rapidly while the rest of the scene remains coherent.

Under traditional SIMT rasterization, all 256,000 pixels get pushed through the pipeline regardless. Under TWRF, the static parts cost almost zero compute.

---

## How It Works

1. **Spatial Tiling:** The 640x400 display is broken down into a clean grid of 16x16 pixel tiles (40 columns by 25 rows = 1,000 tiles per frame).
2. **On-Chip State Store:** The virtual GPU reserves a dedicated 1 MB pool of persistent SRAM. Each tile gets its own cached output slot.
3. **Change Detection:** At the start of a frame, TWRF checks each tile. If the underlying scene data hasn't moved, the tile's compute kernel is bypassed completely.
4. **Instant Reuse:** The final frame simply reads the cached output from the state store for skipped tiles and blends in newly rendered tiles for dirty regions.

---

## Real Numbers from Real Gameplay

We ran an automated 150-frame timedemo benchmark across Episode 1, Mission 1 (*Hangar*) using genuine id Software shareware assets (`doom1.wad`).

| Metric | Result | What It Means |
| :--- | :--- | :--- |
| **Average Tiles Skipped** | **39.78%** | About 4 out of every 10 tiles never needed recalculation. |
| **HUD Persistence** | **94.90%** | The status bar was almost completely reused across frames. |
| **Cycle Reduction** | **33.7%** | A one-third reduction in total rendering work. |
| **Measured Speedup** | **1.51x** | TWRF ran 51% faster than full-frame recomputation. |

Visual artifacts and charts from this run are saved in the `results/` folder:
- `doom_twrf_benchmark.png`: Frame-by-frame skip ratio and cumulative cycle curves.
- `doom_twrf_benchmark.json`: Raw telemetry data for all 150 frames.
- `doom_frame*.png`: Visual snapshots of cold start, intro screen, and active combat.

---

## Getting Started

### Prerequisites
- A C++20 compiler (GCC 11+, Clang 13+, or MSVC 2019+). On Windows, MSYS2 UCRT64 GCC is recommended.
- CMake 3.20+ and Ninja.
- Python 3 with `matplotlib` and `Pillow` (for generating charts and converting frame dumps).

### Playing DOOM (Interactive Window)
To launch and play DOOM interactively in a window on your desktop with keyboard controls:
```powershell
.\build\twrf_doom.exe
```
Controls:
- **Move:** Arrow Keys or W / A / S / D
- **Fire:** Ctrl
- **Open Doors / Use:** Space
- **Strafe:** A / D
- **Run:** Shift
- **Game Menu:** Esc
- **Weapons:** 1 - 7

To jump straight into Episode 1 Mission 1 (Hangar):
```powershell
.\build\twrf_doom.exe -warp 1 1
```
While playing, the window title bar continuously displays live TWRF metrics showing the current frame, the percentage of 16x16 tiles skipped, and whether the HUD status bar was cached.

### Automated Benchmark Mode
To run the automated 150-frame headless timedemo benchmark and generate performance metrics:
```powershell
.\build\twrf_doom.exe --bench
```

### One-Click Pipeline Run (Windows)
Run the automated pipeline script from PowerShell:
```powershell
.\run.ps1
```
This builds the simulator, runs all 35 acceptance tests, executes the core demo, runs the 150-frame DOOM benchmark, and generates fresh comparison graphs in `results/`.

---

## Project Structure

- `src/doom/doomgeneric_twrf.cpp`: The bridge connecting the DOOM engine to TWRF's tile cache and state store.
- `doomgeneric/`: Port-friendly C sources of classic DOOM.
- `doom1.wad`: Genuine id Software Shareware game data (Episode 1: *Knee-Deep in the Dead*).
- `include/twrf/`: Core virtual GPU headers (state store, schedulers, change trackers, cost models).
- `src/core/`: Implementation of the TWRF virtual GPU architecture.
- `tests/`: 35 rigorous CTest suites testing determinism, cache invalidation, and oracle parity.
- `python/analysis/`: Lightweight visualization scripts that produce comparison curves.
- `results/`: Output benchmarks, telemetry logs, and rendered graphs.

---

## Correctness & Guarantees

Incremental computing only matters if it looks right. TWRF includes a formal verification test suite with 35 automated checks to guarantee:
- **Zero False Negatives:** If a region changed, it is never mistakenly skipped.
- **Oracle Parity:** The output of incremental TWRF execution is bit-identical to running a full recomputation from scratch.
- **Deterministic Scheduling:** Multi-threaded or out-of-order region completion always yields the same final frame.

---

## Licensing & Intellectual Property

This project incorporates components distributed under distinct open-source and proprietary licenses:

### 1. TWRF Architecture & Simulator (MIT License)
The Temporal Work Region Fabric (TWRF) virtual GPU core simulator, scheduling abstractions, cost models, test suites, and analysis tools are open-source software licensed under the **MIT License**.
- Permissive terms: you are free to use, copy, modify, merge, publish, distribute, and sublicense the code.
- See the repository [`LICENSE`](LICENSE) file for the complete legal text.

### 2. DOOM Engine Subsystem (GNU General Public License v2)
The game engine integration in `doomgeneric/` and associated port bindings are derived from the 1997 id Software DOOM source code release, governed by the **GNU General Public License v2.0 (GPL-2.0)**.
- Reciprocal terms: any redistribution or modification of the DOOM engine subsystem must remain open source under the GNU GPL v2.
- For complete terms and conditions, refer to [`doomgeneric/LICENSE`](doomgeneric/LICENSE) or the Free Software Foundation GNU registry.

### 3. id Software DOOM Assets (Shareware v1.9)
The game data file `doom1.wad` contains the original shareware levels (Episode 1: *Knee-Deep in the Dead*) created by id Software (Copyright 1993-1996 id Software LLC).
- Provided for non-commercial evaluation, benchmarking, and academic reproducibility.
- Commercial game episodes require registered IWAD files purchased from id Software / Bethesda.
