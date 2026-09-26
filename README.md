# Temporal Work Region Fabric (TWRF)

Temporal Work Region Fabric (TWRF) is a **research virtual-GPU architecture and simulator** for studying persistent, spatially bounded computational work across frames.

The central abstraction is the **Temporal Work Region (TWR)**: a persistent execution object with stable identity, spatial extent, persistent state/output, versioned inputs, explicit dependencies, and execution state.

\[
R_i=(ID_i,A_i,S_i,I_i,O_i,V_i,\Sigma_i,D_i,Q_i)
\]

The research question is not whether incremental computation is possible. That is established by prior work. The question is whether making a persistent spatial work object a first-class hardware scheduling primitive can reduce incremental-management cost relative to both full recomputation and an equivalent software-managed incremental runtime.

\[
\boxed{
Persistent\ Spatial\ Work\ Identity
+
Persistent\ State
+
Version/Validity\ Tracking
+
Explicit\ Dependency\ Graph
+
Hardware-Oriented\ Scheduling
}
\]

See the prior-art map in docs/PRIOR_ART.md for the claim boundary.

## Architecture

The simulator evaluates three workload classes under one execution contract:

- **Raster:** spatial screen tiles with persistent tile outputs.
- **Ray:** bounded ray batches with versioned camera, geometry, and light inputs.
- **Neural:** small MLP reconstruction blocks with persistent temporal output state.

The heterogeneous example is:

\[
Raster\ GBuffer \rightarrow Ray\ Shadow \rightarrow Neural\ Denoise
\]

with direct Raster -> Neural and Ray -> Neural dependencies.

The persistent logical state store represents the architectural location where valid TWR outputs survive frame boundaries.

## Correctness model

Correctness is a first-class research gate.

A TWR becomes clean only after successful kernel completion, dependency-audit validation, and successful State Store output commit.

\[
ObservedMutableDependencies(R_i)
\subseteq
DeclaredDependencies(R_i)
\]

The corresponding false-negative invalidation metric is:

\[
FNI=
\frac{Required\ invalidations\ missed}
{Required\ invalidations}
\]

and the acceptance criterion is:

\[
\boxed{FNI=0}
\]

Conservative false-positive invalidation is permitted and measured separately.

Every incremental raster experiment also has a forced full-recompute oracle. Software Baseline C must execute the same semantic workset as TWRF before timing comparisons are interpreted.

## Experimental baselines

### Baseline A -- Full recomputation

Every region is recomputed every frame:

\[
C_A=C_r
\]

### Baseline B -- Temporal cache

A conventional reuse model with cache lookup, validation, and miss/refill overhead.

### Baseline C -- Software incremental runtime

A concrete software scheduler performs explicit version checks, dependency propagation, software ready-set management, the same TWR kernels, the same persistent-state semantics, and the same mutation trace.

The principal architectural comparison is:

\[
C_{TWRF} \stackrel{?}{<} C_{B3}
\]

not merely whether TWRF can beat full recomputation on temporally coherent workloads.

## Measurement discipline

The simulator records measured control-plane and State Store quantities separately from timing-model outputs.

### Measured simulator quantities

Examples include TWR executions and skips, resource-version checks, producer-version checks, spatial bounding checks, dependency propagations/traversals, ready-set/queue pushes and pops, failed executions, State Store reads/writes, State Store bytes, execution traces, and dependency-audit outcomes.

### Derived quantities

Architectural cycle estimates are derived from parameterized timing constants:

\[
C=C_{compute}+C_{detect}+C_{schedule}+C_{dependency}+C_{state}+C_{memory}+C_{interconnect}
\]

They are **not measurements of a physical GPU**.

### Hardware estimates

FPGA resource figures and RTL mappings in docs/FPGA_FEASIBILITY.md are design estimates, not synthesized silicon measurements.

## Experimental result

The validated 14-case raster matrix shows execution/output parity and zero dependency-audit failures. Under the default timing parameters, TWRF has lower derived cost than the executable software incremental baseline in all 14 cases, while the temporal-cache baseline remains lower-cost than TWRF and full recomputation remains lower-cost for every non-static case. The 135-setting sensitivity campaign also places TWRF below Baseline C throughout the tested grid. These are derived timing-model results, not physical-GPU benchmarks.

## Validation gates

The project uses the following evidence ladder:

1. semantic lifecycle correctness
2. dependency soundness
3. full-recompute output equivalence
4. B3 execution-set parity
5. B3 output parity
6. measured simulator accounting
7. parameterized timing comparison
8. locality and volatility sweeps
9. sensitivity analysis
10. hardware realization study.

See docs/RESEARCH_VALIDATION_GATE.md.

## GTA 3 3D Virtual GPU Workload (Zero Host GPU Usage)

The `twrf-gta3` branch introduces an end-to-end 3D polygonal virtual-GPU workload targeting Grand Theft Auto III urban rendering with **zero host GPU hardware utilization** (no DirectX, Vulkan, OpenGL, or CUDA hardware calls).

### Key Architectural Additions

1. **3D Spatial Binning (`SpatialBinner3D`)**:
   Projects 3D entity oriented bounding boxes (AABBs) to 2D screen tiles using conservative projection matrices. Only tiles intersected by moving entities (e.g. player sedan, ambient taxi) are invalidated. Background city geometry tiles remain cached with zero re-rasterization.
2. **Deep 3D State Store (`LogicalStateStore3D`)**:
   Maintains dual-plane slot memory per $16 \times 16$ tile: RGBA32 color buffer ($1\text{ KB}$) and Z32 depth buffer ($1\text{ KB}$). Performs hardware-oriented depth-range testing (`is_depth_occluded`) to cull occluded geometry prior to rasterization.
3. **Dual-Layer Compositor (`DualLayerCompositor`)**:
   Decouples 3D perspective world geometry from 2D orthographic HUD layers (minimap radar, health/armor meters, wanted stars). HUD updates selectively invalidate only HUD tiles without invalidating underlying 3D world geometry.

### Benchmark Telemetry Summary

Deterministic 180-frame driving benchmark at $960 \times 540$ resolution ($2040$ total tiles, $16 \times 16$ tile size):

| Metric | Measured Value |
|---|---|
| **Average Temporal Tile Reuse** | **97.35 %** |
| **Mean TWRF Incremental Frame Time** | **61.20 ms (16.3 FPS on pure CPU)** |
| **Mean Baseline Full-Frame Raster Time** | **2898.65 ms (0.3 FPS on pure CPU)** |
| **Cumulative Speedup Factor** | **47.37x** |
| **Host GPU Utilization** | **0.00 % (Pure TWRF Software Virtual GPU)** |

### Visual Artifacts

- **3D City Render Preview:** `results/gta3_city_preview.png`
- **Benchmark Speedup Telemetry:** `results/gta3_twrf_speedup.png`
- **Machine-Readable Telemetry:** `results/gta3_twrf_benchmark.json`

## Research documentation

- [TWRF architecture specification](docs/TWRF_SPEC.md)
- [Formal execution semantics](docs/SEMANTICS.md)
- [Research validation gates](docs/RESEARCH_VALIDATION_GATE.md)
- [Experimental results](docs/RESULTS.md)
- [Prior-art and originality boundary](docs/PRIOR_ART.md)
- [Research claim matrix](docs/RESEARCH_CLAIM_MATRIX.md)
- [Reproducibility manifest](docs/REPRODUCIBILITY_MANIFEST.md)
- [Research freeze record](docs/RESEARCH_FREEZE.md)
- [Limitations and threats to validity](docs/LIMITATIONS.md)
- [FPGA/RTL feasibility](docs/FPGA_FEASIBILITY.md)
- [Publication paper draft](docs/PAPER_DRAFT.md)
- [Figures and tables plan](docs/FIGURES_AND_TABLES.md)
- [Appendix assembly](docs/APPENDIX_INDEX.md)
- [Publication freeze record](docs/PUBLICATION_FREEZE.md)

## Running the project

### CMake Build & Test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### Automated GTA3 Runner (PowerShell)

```powershell
.\run.ps1
```

### Running TWRF GTA3 3D Benchmarks

```bash
# Run 180-frame headless benchmark (exports telemetry and preview images)
./build/twrf_city_3d.exe --bench 180

# Generate publication-grade speedup plots
python python/analysis/plot_gta3_results.py
```

### Interactive Real-Time 3D Exploration (Win32)

```bash
# Run real-time interactive simulation
./build/twrf_city_3d.exe
```

- **W / S / Up / Down:** Accelerate / Reverse
- **A / D / Left / Right:** Steer Left / Right
- **C:** Cycle Camera (Follow Cam, Sidewalk Surveillance Cam, Rooftop Cam)
- **T:** Toggle Tile Debug Borders (Green = Cached, Red = Rasterized)
- **H:** Toggle HUD Overlay
- **ESC:** Exit

## Repository structure

```text
include/twrf/core/       TWR semantics, 3D state store, graph, scheduler, audits
include/twrf/raster/     tiled raster workload, 3D spatial binner, dual-layer compositor
include/twrf/ray/        bounded ray workload
include/twrf/neural/     deterministic MLP workload
include/twrf/api/        heterogeneous pipeline
include/twrf/timing/     baselines, cycle accounting, experiment runner
src/gta3/                TWRF GTA3 3D City prototype and virtual GPU renderer
tests/                   regression, research-gate, and 3D spatial binning tests
docs/                    architecture, methodology, prior art, limitations
python/analysis/         result analysis and plotting
results/                 generated experiment output and 3D benchmarks
```

## Final claim boundary

The strongest defensible statement supported by the architecture and simulator is:

> TWRF implements and evaluates a persistent spatial work-region execution model in which region identity, state, validity, dependencies, and output survive across frames. The simulator compares this organization with full recomputation, temporal caching, and an equivalent software incremental scheduler under identical mutation traces. The architectural benefit is workload- and parameter-dependent and must be established from measured simulator operation counts and an explicitly parameterized timing model rather than assumed from temporal coherence alone.
