# Temporal Work Region Fabric (TWRF) -- GTA 3 Edition

Running authentic 3D open-world games entirely on the CPU with **0% GPU hardware usage**.

Instead of recalculating every pixel and polygon every frame, TWRF divides the screen into small tiles ($16 \times 16$ pixels) and memoizes their contents. When a car drives through Liberty City, the buildings, roads, and sky behind it remain unchanged in memory. TWRF only redraws the small set of tiles that actually mutated, giving massive performance gains on pure CPU software rendering.

---

## Key Results at a Glance

Evaluated on a deterministic 180-frame driving trace in an urban 3D city at $960 \times 540$ resolution ($2,040$ total tiles):

| Metric | Conventional Software Rendering | TWRF Virtual GPU | Result |
|---|---|---|---|
| **Average Tile Reuse** | 0.0% (Redraws everything) | **97.35%** | **Over 97% of tiles reused** |
| **Average Frame Time** | 2,898.65 ms (~0.3 FPS) | **61.20 ms (~16.3 FPS)** | **47.37x faster** |
| **Tiles Rendered / Frame** | 2,040 tiles | **54.1 tiles** | **1,985.9 tiles skipped** |
| **Host GPU Usage** | 0% | **0%** | **Pure CPU software execution** |
| **All Automated Tests** | - | **37 / 37 passed (100%)** | **Fully verified** |

---

## Two Ways to Run It

This repository includes two complete ways to test the architecture:

1. **Authentic Rockstar Grand Theft Auto III:** Runs the original commercial PC release of GTA 3 using our drop-in `d3d8.dll` driver.
2. **Built-in 3D City Benchmark (`twrf_city_3d`):** A standalone, interactive 3D city simulator with driving physics, camera modes, and real-time tile caching visualizers.

---

## How It Works (In Plain English)

### 1. 3D Spatial Binning (`SpatialBinner3D`)
When a 3D car moves in the world, TWRF projects its 3D bounding box onto the 2D screen. Only the tiles that overlap with the car's bounding box are marked as dirty. All other tiles (buildings, trees, pavement) are left untouched.

### 2. Deep Dual-Plane Tile Store (`LogicalStateStore3D`)
Each $16 \times 16$ tile stores two layers of data:
- **Color Buffer:** 256 pixels of RGBA color ($1\text{ KB}$).
- **Depth Buffer:** 256 pixels of floating-point depth ($1\text{ KB}$).

When geometry is tested against a tile, TWRF checks the depth range. If a car drives behind a building, TWRF recognizes that the building is closer, culling the car before spending CPU cycles rendering it.

### 3. Decoupled HUD and 3D World (`DualLayerCompositor`)
Games have 2D heads-up displays (health bars, radar minimaps, text) that sit on top of the 3D world. TWRF keeps HUD tiles separate from 3D world tiles. When your radar blinks, TWRF updates only the radar tiles without invalidating the 3D buildings underneath.

### 4. Direct3D 8 Interceptor (`d3d8.dll`)
GTA 3 communicates with your graphics card using Microsoft Direct3D 8. On Windows, applications look for DLL files in their own folder before checking system folders. Placing our custom `d3d8.dll` in the game folder intercepts all 3D draw calls from Rockstar's RenderWare engine:
- Catches vertex buffers, index buffers, and textures in memory.
- Routes 3D meshes and 2D HUD sprites into TWRF's tile cache.
- Blits the finished frame directly to the window using standard Windows GDI.
- Zero calls to physical GPU drivers (no DirectX, Vulkan, OpenGL, or CUDA).

---

## Quickstart

### 1. Build the Project and Run Tests

Prerequisites: CMake 3.20+ and a C++20 compiler (GCC, Clang, or MSVC).

```bash
# Configure and build
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run all 37 tests
ctest --test-dir build --output-on-failure
```

---

### 2. Run the Interactive 3D City Simulator

Test the architecture right away with our built-in 3D driving sandbox:

```bash
./build/twrf_city_3d.exe
```

**Controls:**
- **W / S / Up / Down:** Accelerate / Reverse
- **A / D / Left / Right:** Steer Left / Right
- **C:** Switch Camera (Follow Cam, Sidewalk Cam, Rooftop Cam)
- **T:** Toggle Tile Visualizer (Green = Reused from cache, Red = Redrawn)
- **H:** Toggle HUD (Minimap, Health bar, Wanted stars)
- **ESC:** Exit

To run the automated 180-frame benchmark without graphics:
```bash
./build/twrf_city_3d.exe --bench 180
python python/analysis/plot_gta3_results.py
```

---

### 3. Run the Authentic Rockstar GTA 3 Game

To run the commercial release of GTA 3 on the TWRF virtual GPU:

#### Step A: Compile the 32-bit D3D8 Driver
Because GTA 3 is a 32-bit executable (`x86`), the DLL must be compiled as a 32-bit library:

```bash
# Using 32-bit MinGW GCC (i686-w64-mingw32-g++)
g++ -shared -O3 -std=c++20 -static -static-libgcc -static-libstdc++ -Wl,--kill-at \
    -I include -I . \
    src/core/twrf_core.cpp src/d3d8/twrf_d3d8.cpp \
    -lgdi32 -luser32 \
    -o d3d8.dll
```

#### Step B: Install the Game Files
If you do not have the game installed, run the automated preservation downloader:
```bash
python game/download_gta3.py
```
This extracts the authentic game files (`models/gta3.img`, `data/`, `txd/`, `anim/`, `audio/`, `gta3.exe`).

#### Step C: Launch GTA 3
Place the compiled `d3d8.dll` in the game folder next to `gta3.exe` and run:
```bash
./gta3.exe -nointro
```

You can view the live execution log in `twrf_gta3.log`, which tracks every device creation, texture load, vertex buffer allocation, and frame presentation.

---

## Automated Acceptance Test Suite

The repository includes 37 automated tests verifying every layer of the architecture:

- **Tests 1-13:** Core work region lifecycle, dependency graph, cost accounting, state store.
- **Tests 14-20:** Tiled rasterization, tile cache reuse, repeatability, dynamic object transforms.
- **Tests 21-28:** Baselines parity, break-even validation, oracle comparison.
- **Test 29:** 135-case multidimensional parameter sensitivity sweep.
- **Tests 30-35:** Heterogeneous workloads (ray tracing batches, neural MLP inference, cross-workload DAGs).
- **Test 36:** 3D spatial binning, AABB projection, and depth occlusion testing.
- **Test 37:** Direct3D 8 interception, COM interface verification, and device creation.

All 37 tests pass with 100% reliability (`ctest --test-dir build`).

---

## Repository Layout

```text
include/twrf/
  core/         TWR lifecycle, 3D dual-plane state store, dependency scheduler
  d3d8/         Direct3D 8 COM interface wrappers and interception headers
  raster/       Tiled rasterizer, 3D spatial binner, dual-layer compositor
  ray/          Bounded ray tracing workload
  neural/       Deterministic neural inference workload
  api/          Pipeline orchestration and heterogeneous execution
  timing/       Cycle accounting model and timing parameters
src/
  core/         Core TWRF runtime and state tracking
  d3d8/         Direct3D 8 virtual GPU implementation (d3d8.dll)
  gta3/         TWRF 3D City benchmark and interactive simulator
tests/          All 37 unit, regression, 3D, and D3D8 test harnesses
game/           Automated game preservation downloader and extraction script
results/        Benchmark outputs, speedup curves, and render previews
```

---

## Research Claim Boundary

TWRF evaluates whether making a persistent spatial work region a first-class execution primitive can eliminate redundant rendering in continuous 3D environments. Speedup figures are measured operation counts and cycle models from identical workload traces comparing TWRF against full recomputation and software caching baselines.
