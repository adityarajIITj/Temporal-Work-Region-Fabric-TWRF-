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

with direct Raster → Neural and Ray → Neural dependencies.

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

### Baseline A — Full recomputation

Every region is recomputed every frame:

\[
C_A=C_r
\]

### Baseline B — Temporal cache

A conventional reuse model with cache lookup, validation, and miss/refill overhead.

### Baseline C — Software incremental runtime

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

## Running the project

### CMake

\`\`\`bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
\`\`\`

### Demo and experiment generation

\`\`\`bash
./build/twrf_demo
python python/analysis/plot_break_even.py
\`\`\`

The demo writes machine-readable experimental output under results/.

## Repository structure

\`\`\`text
include/twrf/core/       TWR semantics, graph, scheduler, state store, audits
include/twrf/raster/     tiled raster workload and full-recompute oracle
include/twrf/ray/        bounded ray workload
include/twrf/neural/     deterministic MLP workload
include/twrf/api/        heterogeneous pipeline
include/twrf/timing/     baselines, cycle accounting, experiment runner
tests/                   regression and research-gate tests
docs/                    architecture, methodology, prior art, limitations
python/analysis/         result analysis and plotting
results/                 generated experiment output
\`\`\`

## Final claim boundary

The strongest defensible statement supported by the architecture and simulator is:

> TWRF implements and evaluates a persistent spatial work-region execution model in which region identity, state, validity, dependencies, and output survive across frames. The simulator compares this organization with full recomputation, temporal caching, and an equivalent software incremental scheduler under identical mutation traces. The architectural benefit is workload- and parameter-dependent and must be established from measured simulator operation counts and an explicitly parameterized timing model rather than assumed from temporal coherence alone.
