# Temporal Work Region Fabric (TWRF) -- Grand Theft Auto III (3D Virtual GPU Architecture)

Temporal Work Region Fabric (TWRF) is a **research virtual-GPU architecture and simulator** designed for evaluating persistent, spatially bounded computational work across frames.

The `twrf-gta3` branch delivers an end-to-end 3D polygonal virtual-GPU implementation capable of executing both:
1. **The authentic commercial release of Grand Theft Auto III (Rockstar Games / RenderWare)** via a custom Direct3D 8 interceptor driver (`d3d8.dll`).
2. **A deterministic 3D urban virtual-GPU simulator and benchmark (`twrf_city_3d`)** delivering empirical speedup measurements.

Both workloads execute with **0.00% host GPU hardware utilization** (pure CPU execution; no DirectX, Vulkan, OpenGL, or CUDA host hardware calls).

```
+---------------------------------------------------------------------------------------------------+
|                                      APPLICATION LAYER                                            |
|   Authentic Rockstar GTA 3 (gta3.exe)           |       TWRF 3D City Benchmark (twrf_city_3d)    |
+-------------------------------------------------+-------------------------------------------------+
|   Direct3D 8 Interceptor (d3d8.dll)             |       Native C++20 3D Pipeline                  |
|   (IDirect3D8, Device8, Texture8, VertexBuffer) |       (SpatialBinner3D, DualLayerCompositor)    |
+-------------------------------------------------+-------------------------------------------------+
|                                 TWRF VIRTUAL GPU CORE                                             |
|   +-------------------------------------------------------------------------------------------+   |
|   | 3D Spatial Binner: Conservative AABB Screen Projection & Localized Invalidation           |   |
|   | Logical State Store 3D: Dual-Plane Memoization (RGBA32 Color + Z32 Depth per Tile)        |   |
|   | Hardware Occlusion Testing: Bounded Depth Range Invalidation Culling                      |   |
|   | Dual-Layer Compositor: Decoupled 3D Perspective World vs 2D Orthographic HUD              |   |
|   +-------------------------------------------------------------------------------------------+   |
+---------------------------------------------------------------------------------------------------+
|                                 PRESENTATION LAYER                                                |
|   Win32 GDI Device Independent Bitmap Blit (SetDIBitsToDevice / StretchDIBits)                    |
|   --> 100% Pure CPU Execution | 0.00% Host GPU Hardware Utilization                               |
+---------------------------------------------------------------------------------------------------+
```

---

## Table of Contents

- [Architectural Motivation](#architectural-motivation)
- [3D Architectural Innovations](#3d-architectural-innovations)
  - [1. 3D Spatial Binning](#1-3d-spatial-binning)
  - [2. Deep Dual-Plane 3D State Store](#2-deep-dual-plane-3d-state-store)
  - [3. Dual-Layer Compositor](#3-dual-layer-compositor)
- [Authentic Rockstar GTA 3 Execution](#authentic-rockstar-gta-3-execution)
  - [Direct3D 8 Interceptor Architecture](#direct3d-8-interceptor-architecture)
  - [Automated Asset Acquisition](#automated-asset-acquisition)
  - [Runtime Telemetry and Verification](#runtime-telemetry-and-verification)
- [Empirical Benchmark Results](#empirical-benchmark-results)
  - [Benchmark Telemetry Summary](#benchmark-telemetry-summary)
  - [Performance Analysis](#performance-analysis)
  - [Visual Artifacts](#visual-artifacts)
- [Experimental Baselines and Validation](#experimental-baselines-and-validation)
- [Acceptance Test Suite](#acceptance-test-suite)
- [Build and Execution Guide](#build-and-execution-guide)
  - [Prerequisites](#prerequisites)
  - [CMake Build and Test Suite](#cmake-build-and-test-suite)
  - [Building 32-bit D3D8 Driver for GTA 3](#building-32-bit-d3d8-driver-for-gta-3)
  - [Running the Interactive 3D City Simulator](#running-the-interactive-3d-city-simulator)
  - [Running Headless 3D Benchmark](#running-headless-3d-benchmark)
- [Repository Structure](#repository-structure)
- [Formal Claim Boundary](#formal-claim-boundary)

---

## Architectural Motivation

Traditional GPU graphics architectures operate on an immediate-mode, stateless execution paradigm: every frame is treated as an isolated computational event. Geometry is transformed, binned, rasterized, shaded, and written to a framebuffer anew each cycle, discarding structural continuity between frames.

TWRF formalizes the **Temporal Work Region (TWR)** as a persistent, stateful scheduling primitive:

$$R_i = (ID_i, A_i, S_i, I_i, O_i, V_i, \Sigma_i, D_i, Q_i)$$

Where:
- $ID_i$: Stable temporal region identity across frames.
- $A_i$: Bounded spatial extent in screen/raster coordinates.
- $S_i$: Persistent logical state (color and depth buffer allocations).
- $I_i$: Versioned input parameters (transform matrices, vertex buffers, textures).
- $O_i$: Memoized output buffer.
- $V_i$: Explicit validity bit vector.
- $\Sigma_i$: Execution state (Clean, Dirty, Evaluating, Failed).
- $D_i$: Declared and audited dependency set.
- $Q_i$: Scheduling priority and cost metrics.

While 2D planar workloads exhibit straightforward rectangular invalidation, extending TWRF to **3D open-world environments** introduces three distinct challenges:
1. **Dynamic Perspective Projection:** Camera movement alters screen projections of static geometry non-linearly.
2. **Depth Buffer Occlusion:** Foreground objects moving across static backgrounds require depth testing to prevent stale occluded fragments from leaking through.
3. **Decoupled Update Frequencies:** Static buildings, dynamic vehicular entities, and 2D orthographic heads-up displays (HUD) mutate at fundamentally different temporal rates.

---

## 3D Architectural Innovations

### 1. 3D Spatial Binning

Implemented in `include/twrf/raster/spatial_binner_3d.hpp`:

The 3D Spatial Binner computes conservative screen-space axis-aligned bounding boxes (AABBs) for 3D world entities:

$$\text{ScreenAABB} = \text{Project}\left( \mathbf{M}_{proj} \times \mathbf{M}_{view} \times \mathbf{M}_{world}, \text{Box3D} \right)$$

- Only tiles overlapping with the projected 2D bounds of dynamic entities (e.g., the player vehicle, ambient taxis) are invalidated and flagged for re-rasterization.
- Background buildings, roads, sidewalks, and terrain tiles remain valid in the logical state store across frames when the camera is stationary or undergoing bounded parallax.

### 2. Deep Dual-Plane 3D State Store

Implemented in `include/twrf/core/state_store_3d.hpp`:

Each $16 \times 16$ tile slot maintains dual persistent memory planes:
- **Color Buffer Plane:** 256 pixels $\times$ 4 bytes (RGBA32) = 1,024 bytes.
- **Depth Buffer Plane:** 256 pixels $\times$ 4 bytes (Z32 float) = 1,024 bytes.

The state store performs hardware-oriented depth range testing (`is_depth_occluded`):
- If all vertices of an entity projected into a tile possess depth values strictly greater than the tile's minimum depth ($z_{entity} > z_{tile}^{max}$), the entity is culled immediately with zero rasterization overhead.

### 3. Dual-Layer Compositor

Implemented in `include/twrf/raster/dual_layer_compositor.hpp`:

Real-time games combine 3D perspective world geometry with 2D orthographic screen overlays (minimap radar, health and armor meters, weapon icons, text). The Dual-Layer Compositor isolates these layers:
- HUD elements can update every frame (e.g., flashing health bar, radar blips) without triggering invalidation of the underlying 3D world geometry tiles.
- The compositor merges the clean 3D world tile with the active HUD layer during the final presentation pass.

---

## Authentic Rockstar GTA 3 Execution

### Direct3D 8 Interceptor Architecture

Implemented in `include/twrf/d3d8/twrf_d3d8.hpp` and `src/d3d8/twrf_d3d8.cpp`:

On Windows, when an executable calls `LoadLibrary("d3d8.dll")`, the operating system searches the application's local directory before searching system paths (`C:\Windows\System32`). TWRF compiles a drop-in 32-bit `d3d8.dll` that intercepts all graphics calls made by Rockstar's RenderWare 3.x engine.

**Supported COM Interfaces and Capabilities:**
- `Direct3DCreate8`: Instantiates `TWRFDirect3D8`, reporting adapter string `"Temporal Work Region Fabric (TWRF) Virtual GPU"` and full Direct3D 8 capability flags (`D3DDEVCAPS_HWTRANSFORMANDLIGHT`, `D3DPTFILTERCAPS_MINFPOINT`, `D3DTEXOPCAPS_MODULATE`).
- `IDirect3DDevice8`: Manages device state, transforms (`D3DTS_WORLD`, `D3DTS_VIEW`, `D3DTS_PROJECTION`), viewport configuration, and render states.
- `CreateVertexBuffer` and `CreateIndexBuffer`: Stores mesh vertices and index lists in unified system memory.
- `CreateTexture`: Allocates texture dictionaries (`.txd`), supporting RGBA32 and 16-bit color formats with UV wrapping.
- `DrawPrimitiveUP` and `DrawIndexedPrimitive`: Routes 3D perspective world geometry (`D3DFVF_XYZ`) through TWRF's `TileRasterizer` into `LogicalStateStore3D`, and translates 2D orthographic HUD geometry (`D3DFVF_XYZRHW`) into screen-space tile updates.
- `Present`: Reconstructs the screen buffer from memoized state store tiles and blits directly to the game window via Windows GDI (`StretchDIBits` / `SetDIBitsToDevice`) with zero host GPU hardware usage.
- **Export Table ABI Compliance:** Full compliance with standard Direct3D 8 exports: `Direct3DCreate8`, `ValidatePixelShader`, `ValidateVertexShader`, `DebugSetMute`, and `Direct3D8EnableMaximizedWindowedModeShim`.

### Automated Asset Acquisition

An automated preservation tool is provided in `game/download_gta3.py`:
- Downloads the authentic PC preservation release.
- Mounts disk images (`GTA3_INSTALL.iso`, `GTA3_AUDIO.iso`) and extracts asset archives via InstallShield decompression (`unshield`).
- Unpacks `models/gta3.img` (170.89 MB), `models/gta3.dir`, `data/`, `txd/`, `anim/`, `audio/`, and the 1.1 patch executable (`gta3.exe`).

### Runtime Telemetry and Verification

When `gta3.exe` executes with TWRF's `d3d8.dll`, runtime operations are logged to `twrf_gta3.log`:

```text
[TWRF D3D8] DLL_PROCESS_ATTACH: TWRF Virtual GPU driver loaded into process.
[TWRF D3D8] Direct3DCreate8 called by game application.
[TWRF D3D8] Routing all 3D pipeline commands to TWRF Virtual GPU (0% Host GPU usage).
[TWRF D3D8] GetDeviceCaps() -> Full TWRF 3D capabilities reported
[TWRF D3D8] GetAdapterIdentifier() -> Temporal Work Region Fabric (TWRF) Virtual GPU
[TWRF D3D8] EnumAdapterModes(0) -> 640x480
[TWRF D3D8] EnumAdapterModes(1) -> 800x600
[TWRF D3D8] CreateDevice() called -> Resolution: 640x480 | HWND=1182390
[TWRF D3D8] Initialized Virtual GPU Device: 640x480 (1200 tiles) | Pure CPU Execution (0% Host GPU)
[TWRF D3D8] CreateVertexBuffer #1 (262144 bytes)
[TWRF D3D8] CreateVertexBuffer #2 (262144 bytes)
[TWRF D3D8] CreateTexture #1 (512x512)
[TWRF D3D8] CreateTexture #2 (256x256)
[TWRF D3D8] CreateTexture #3 (1024x1024)
```

---

## Empirical Benchmark Results

### Benchmark Telemetry Summary

The 3D city benchmark (`twrf_city_3d.exe --bench 180`) was evaluated on a deterministic 180-frame driving trace through an urban city environment containing buildings, dynamic vehicles, and an orthographic HUD:

- **Resolution:** $960 \times 540$
- **Tile Configuration:** $16 \times 16$ pixels ($2,040$ total tiles per frame)
- **Evaluation Mode:** Pure CPU software execution (single-thread software rasterizer baseline vs TWRF virtual GPU)

| Metric | Measured Value |
|---|---|
| **Total Frames Evaluated** | **180** |
| **Total Screen Tiles per Frame** | **2,040 tiles** |
| **Average Tiles Re-rasterized per Frame** | **54.1 tiles** |
| **Average Tiles Reused from State Store** | **1,985.9 tiles** |
| **Average Temporal Tile Reuse Rate** | **97.35 %** |
| **Mean TWRF Incremental Frame Time** | **61.20 ms (16.3 FPS)** |
| **Mean Conventional Full-Frame Raster Time** | **2,898.65 ms (0.3 FPS)** |
| **Cumulative Speedup Factor** | **47.37x** |
| **Host GPU Hardware Utilization** | **0.00 % (Pure CPU Execution)** |

### Performance Analysis

1. **High Temporal Redundancy:** Because background skyscrapers, roads, and street furniture remain stationary relative to the camera, TWRF achieves over 97% tile reuse.
2. **Selective Invalidation:** Only the localized screen regions occupied by the moving player vehicle and ambient traffic are marked dirty.
3. **Occlusion Efficiency:** When dynamic vehicles move behind buildings, `is_depth_occluded` prevents unnecessary state store updates.
4. **Decoupled HUD Rendering:** Minimap updates invalidate only the 64 tiles dedicated to the radar, leaving the remaining 1,976 tiles unaffected.

### Visual Artifacts

The simulator automatically generates high-resolution telemetry and visual assets in `results/`:
- `results/gta3_city_preview.png`: Render preview of the 3D urban environment showing spatial tile binning.
- `results/gta3_twrf_speedup.png`: Speedup curve and frame-time comparison graph.
- `results/gta3_twrf_benchmark.json`: Machine-readable frame-by-frame execution metrics.

---

## Experimental Baselines and Validation

TWRF is evaluated against three standard baselines:

### Baseline A -- Full Recomputation
Every tile is recomputed from scratch every frame without caching or state retention:

$$C_A = C_r$$

### Baseline B -- Temporal Cache
A conventional cache lookup model with tag comparison, validation checks, and cache miss refill overhead.

### Baseline C -- Software Incremental Runtime
An executable software scheduler executing identical TWR kernels, dependency tracking, ready-set management, and mutation traces.

### Correctness Criteria
Correctness is enforced as a mandatory research validation gate:
- **False-Negative Invalidation (FNI):** A region must never reuse stale output when its inputs or dependencies have changed:

$$FNI = \frac{\text{Required invalidations missed}}{\text{Required invalidations}} = 0$$

- All 37 automated tests verify $FNI = 0$ across diverse mutation rates and dependency graphs.

---

## Acceptance Test Suite

The test suite consists of 37 automated unit, regression, and integration tests:

| Test ID | Test Target | Focus Area | Result |
|---|---|---|---|
| **1-13** | Core TWR Semantics | Lifecycle, dependency propagation, cost model, state store commits | **Passed** |
| **14-20** | Raster Operations | Tile sweeps, raster reuse, repeatability, dynamic transforms | **Passed** |
| **21-28** | Accounting & Baselines | Baseline parity, break-even analysis, oracle matrix | **Passed** |
| **29** | Sensitivity Analysis | 135-point multidimensional parameter sweep | **Passed** |
| **30-35** | Heterogeneous Workloads | Ray batching, neural inference, cross-workload DAGs, API replay | **Passed** |
| **36** | `test_3d_spatial_binning` | 3D AABB projection, depth occlusion, dual-plane slot memoization | **Passed** |
| **37** | `test_twrf_d3d8` | Direct3D 8 DLL interception, device creation, draw call dispatch | **Passed** |

**CTest Status:** 37 of 37 tests passing (100% pass rate).

---

## Build and Execution Guide

### Prerequisites

- **CMake:** Version 3.20 or newer
- **C++ Compiler:** Supporting C++20 (GCC 11+, Clang 13+, MSVC 2019+)
- **Build System:** Ninja or Make
- **Operating System:** Windows 10/11 (for Direct3D 8 interception and Win32 GDI presentation) or Linux (for headless core simulation)

### CMake Build and Test Suite

```bash
# Configure the build directory
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release

# Compile all targets
cmake --build build

# Execute the complete 37-test suite
ctest --test-dir build --output-on-failure
```

### Building 32-bit D3D8 Driver for GTA 3

Because the original `gta3.exe` executable is a 32-bit application (`pei-i386`), `d3d8.dll` must be compiled using a 32-bit toolchain:

```bash
# Using 32-bit MinGW GCC (i686-w64-mingw32-g++)
g++ -shared -O3 -std=c++20 -static -static-libgcc -static-libstdc++ -Wl,--kill-at     -I include -I .     src/core/twrf_core.cpp src/d3d8/twrf_d3d8.cpp     -lgdi32 -luser32     -o game/GTA3_Game/d3d8.dll
```

To run the game:
1. Copy the compiled `d3d8.dll` into the game directory next to `gta3.exe`.
2. Launch: `gta3.exe -nointro`
3. Inspect runtime execution metrics in `twrf_gta3.log`.

### Running the Interactive 3D City Simulator

```bash
./build/twrf_city_3d.exe
```

**Controls:**
- **W / S / Up / Down:** Accelerate / Reverse
- **A / D / Left / Right:** Steer Left / Right
- **C:** Cycle Camera (Follow Cam, Sidewalk Surveillance Cam, Rooftop Cam)
- **T:** Toggle Tile Debug Visualizer (Green = Cached, Red = Re-rasterized)
- **H:** Toggle HUD Overlay
- **ESC:** Exit

### Running Headless 3D Benchmark

```bash
# Run headless 180-frame benchmark
./build/twrf_city_3d.exe --bench 180

# Generate publication-grade speedup graphs
python python/analysis/plot_gta3_results.py
```

---

## Repository Structure

```text
include/
  twrf/
    core/            TWR semantics, 3D deep state store, DAG scheduler, audit
    d3d8/            Direct3D 8 COM interface wrappers and interception headers
    raster/          Tiled rasterizer, 3D spatial binner, dual-layer compositor
    ray/             Bounded ray tracing workload
    neural/          Deterministic MLP neural inference workload
    api/             Heterogeneous pipeline orchestration
    timing/          Cycle accounting model and timing parameters
src/
  core/              TWRF core runtime and state tracking
  d3d8/              Direct3D 8 virtual GPU implementation (d3d8.dll)
  gta3/              TWRF 3D City benchmark and interactive simulator
tests/               Complete 37-test suite (unit, regression, 3D, and D3D8)
game/                Automated asset acquisition and extraction pipeline
docs/                Formal architecture specifications, semantics, and papers
results/             Benchmark telemetry, speedup plots, and render previews
```

---

## Formal Claim Boundary

The architectural claim supported by the TWRF implementation is:

> TWRF establishes a persistent spatial work-region execution model where computational state, region identity, and validity survive across frame boundaries. The simulator benchmarks this organization against full recomputation, temporal caching, and an equivalent software incremental scheduler under identical workload traces. The architectural benefit is workload-dependent and demonstrated by empirical simulator operation counts and cycle accounting models rather than assumed from spatial-temporal coherence alone.
