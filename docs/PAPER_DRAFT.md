# Temporal Work Region Fabric: Persistent Spatial Work Objects for Incremental Virtual-GPU Execution

## Abstract

Frame-wide recomputation is simple, but temporally coherent workloads can contain spatial regions whose inputs remain valid across frames. Temporal Work Region Fabric (TWRF) studies a virtual-GPU organization in which those regions are represented as persistent Temporal Work Regions (TWRs). A TWR retains identity, spatial extent, persistent state/output, versioned inputs, explicit dependencies, and execution state across frames. The simulator implements raster, ray, neural, and heterogeneous workloads under a common contract, together with a forced full-recompute oracle and an executable software incremental scheduler used as Baseline C.

The final software line passes 35/35 registered tests. The standard raster matrix contains 14 clustered/dispersed cases and achieves 14/14 TWRF/Baseline-C execution-set parity, 14/14 output parity, zero dependency-audit failures, and 14/14 full-recompute oracle parity. Under the declared default timing parameters, TWRF has lower derived cost than Baseline C in all 14 cases, while temporal caching remains lower-cost throughout the default matrix and full recomputation remains lower-cost for every non-static case. A 135-setting sensitivity campaign evaluates region size, control-plane cost, and State Store latency. The result is a conditional architectural cost claim, not a universal performance claim.

**Keywords:** incremental computation, virtual GPU, temporal coherence, spatial work regions, dependency scheduling, persistent execution, graphics architecture.

## 1. Introduction

TWRF investigates whether a persistent spatial computation can be represented as a first-class GPU-oriented work object whose identity, state, validity, dependencies, output, and scheduling state persist across frame boundaries. The abstraction is R_i = (ID_i, A_i, S_i, I_i, O_i, V_i, Sigma_i, D_i, Q_i).

The research question is architectural rather than a claim that incremental computation itself is new. Existing work already establishes dependency tracking, memoization, incremental computation, persistent graphics resources, spatial tiling, and incremental rendering. TWRF asks whether their particular GPU-oriented combination has useful architectural properties and a measurable management-cost boundary.

## 2. Research Questions and Contributions

RQ1: Can persistent spatial computation be represented as a first-class execution object?

RQ2: Can selective execution preserve unconditional-recompute output under the declared dependency contract?

RQ3: Can an equivalent software incremental runtime reproduce the same semantic work?

RQ4: Under what timing and architectural assumptions does hardware-oriented management have lower modeled cost than software incremental management?

RQ5: Can one persistent execution contract cover raster, ray, neural, and heterogeneous dependency graphs?

Contributions are the persistent spatial work-object model, explicit lifecycle/validity/dependency semantics, runtime dependency auditing, executable Baseline C, and a reproducible simulator/timing methodology.

## 3. Architecture

A TWR persists across frames: ID_i^t = ID_i^(t+1). Execution is conditional on changed inputs and clean dependencies. Successful execution commits output and advances its version; failed execution does not release dependent work and can roll back prior output state.

The dependency contract is ObservedMutableDependencies(R_i) subset DeclaredDependencies(R_i), with false-negative invalidation targeted at zero.

## 4. Related Work and Positioning

Self-adjusting computation and Adapton establish dependency-aware change propagation and persistent/named incremental computation. Graphics systems establish persistent and temporal resources and spatial tile decomposition. Incremental path-traced rendering establishes affected-region re-rendering. GPU Work Graphs establish GPU-side dynamic work creation and scheduling.

TWRF therefore makes a narrow architectural claim: the persistent entity under study is the spatial computational work object itself, carrying identity, state/output, validity, dependency information, and execution lifecycle across frames. The project does not claim absolute first/only novelty.

## 5. Experimental Methodology

The evaluation uses four configurations: A full recomputation; B temporal cache; C/B3 executable software incremental runtime; and TWRF hardware-oriented persistent-TWR scheduling.

The standard raster experiment uses a 128 x 128 frame, 16 x 16 baseline regions, 64 persistent raster TWRs per frame, 16 benchmark objects, seven mutation levels {0, 0.05, 0.10, 0.25, 0.50, 0.75, 1.0}, and clustered/dispersed locality.

The study distinguishes p_o = mutated objects / objects, p_r = dirty TWRs / TWRs, and p_e = executed TWRs / TWRs. Timing comparisons use p_e.

## 6. Correctness Results

The final software line passes 35/35 registered tests. The standard 14-case matrix establishes 14/14 execution-set parity, 14/14 output parity, zero dependency-audit failures for TWRF and Baseline C, and 14/14 full-recompute oracle parity.

## 7. Execution and Locality Results

At requested p_o = 0.50, clustered mutation produces p_e = 0.125 while dispersed mutation produces p_e = 0.1875. Corresponding TWRF modeled costs are 25,440.8 and 30,634.4 cycles. This demonstrates that spatial mutation structure affects persistent-work reuse.

The static case reaches p_e = 0, but TWRF still incurs 12,633.6 modeled cycles, including 6,080 cycles of change detection and 6,553.6 cycles of State Store activity.

## 8. Architectural Cost Results

The clustered matrix is reported in docs/RESULTS.md. Under the default timing model, TWRF is below Baseline C in all 14 cases. The B3/TWRF derived-cost ratio ranges from 1.6338 to 2.4000. Temporal cache remains below TWRF throughout the default matrix, while full recomputation remains below TWRF at every non-static point.

These are derived timing-model results, not physical-GPU measurements.

## 9. Sensitivity

The final grid contains 3 tile sizes, 9 control-plane multipliers, and 5 State Store latency multipliers: 3 x 9 x 5 = 135 settings. Each evaluates the 14-case matrix, producing 1,890 scenario evaluations. Across the completed sensitivity artifact, TWRF remains below Baseline C throughout the tested grid. The ratio ranges approximately from 1.02598 to 7.91902.

## 10. Cross-Workload Evaluation

Raster, ray, neural, and heterogeneous workloads exercise the same TWR contract. Ray tests versioned camera/geometry/light inputs; neural tests model/input dependencies and temporal state; heterogeneous tests Raster-to-Ray, Raster-to-Neural, and Ray-to-Neural dependencies. These are architectural/generalization evidence rather than equivalent quantitative timing matrices.

## 11. Discussion

The principal trade-off is avoided recomputation versus persistent management. TWRF semantics can be implemented in software, so hardware is not necessary to express the abstraction. The hardware hypothesis is that dedicated organization of validity checking, dependency propagation, scheduling, and persistent-state management can lower incremental control cost.

The temporal-cache result is an important boundary: richer persistent work-object semantics carry additional cost that must be justified by capabilities beyond output reuse alone. TWRF is therefore best understood as an incremental execution fabric rather than a universal replacement for conventional GPU execution.

## 12. Limitations

The evaluation is simulator-based and uses parameterized timing assumptions. No physical GPU, FPGA, or silicon performance measurement is presented. Mutation traces are controlled synthetic workloads. Dependency auditing requires explicit observation by instrumented kernels and is not automatic machine-code memory instrumentation. The quantitative matrix is raster-centric, and Baseline B is an abstraction rather than a commercial cache implementation.

## 13. Conclusion

TWRF provides a persistent spatial work-object model in which region identity, state, validity, dependencies, output, and execution state survive frame boundaries. The implementation establishes semantic correctness and software equivalence in simulation. Under the declared timing model, TWRF has lower modeled cost than the executable software incremental baseline across the evaluated standard matrix and sensitivity grid, while temporal caching and non-static full recomputation remain lower-cost under the current model.

The research position is therefore: TWRF is a hardware-oriented persistent spatial execution architecture whose value is determined by the relationship between avoided computation and persistent management cost. Physical hardware validation is the appropriate next stage.

## References

[1] U. A. Acar, Self-Adjusting Computation, Carnegie Mellon University PhD thesis, 2005.

[2] U. A. Acar et al., A Library for Self-Adjusting Computation, 2006.

[3] M. A. Hammer et al., Adapton: Composable, Demand-Driven Incremental Computation, PLDI, 2014.

[4] M. A. Hammer et al., Incremental Computation with Names, OOPSLA, 2015.

[5] AMD GPUOpen, RPS Tutorial Part 2 — Exploring Render Graphs and RPSL, AMD Render Pipeline Shaders documentation.

[6] Imagination Technologies, Tile-Based Deferred Rendering (TBDR), PowerVR architecture documentation.

[7] P. Hann, Incremental Updates of Path-Traced Scenes during Editing, TU Wien, 2022.

[8] A. Ulschmid, B. Kerbl, K. Krösl, and M. Wimmer, Real-Time Editing of Path-Traced Scenes with Prioritized Re-Rendering, 2024.

[9] A. Ulschmid et al., Automated Prioritization for Context-Aware Re-rendering in Editing, 2025.

[10] Microsoft, Direct3D 12 Work Graphs, DirectX graphics documentation.

Bibliographic metadata should be verified against publisher/venue records before external submission; this draft preserves the repository's current reference set without inventing missing DOI, volume, or page metadata.
