# TWRF Paper Positioning

## Proposed title

**Temporal Work Region Fabric: Persistent Spatial Work Objects for Incremental Virtual-GPU Execution**

## Abstract

Frame-wide recomputation is a simple execution model, but temporally coherent workloads often contain spatial regions whose inputs remain valid across consecutive frames. TWRF studies an alternative GPU organization in which those regions are represented as persistent Temporal Work Regions (TWRs). A TWR retains identity, spatial extent, state/output, versioned inputs, explicit dependencies, and execution state across frames. The simulator implements Raster, Ray, Neural, and heterogeneous workloads under a common TWR contract, together with a forced full-recompute oracle and an executable software incremental scheduler used as Baseline C.

The study explicitly separates semantic correctness from architectural cost. It validates lifecycle safety, dependency soundness, full-recompute output equivalence, and TWRF/Baseline-C execution-set and output parity before interpreting timing results. The standard 14-case raster matrix shows zero dependency-audit failures, complete execution/output parity, and full oracle parity. Under the current default timing parameters, the derived model places TWRF below the executable software incremental baseline in all 14 tested raster cases, while the temporal-cache reference remains lower than TWRF throughout the same matrix. TWRF is below full recomputation only in the completely static case. This result identifies persistent-management overhead as the principal research boundary rather than assuming that temporal reuse alone implies an architectural benefit.

## Research questions

### RQ1 — Representation

Can a persistent spatial computation be represented as a first-class execution object with stable identity, state, validity, dependencies, and output across frames?

### RQ2 — Correctness

Can selective execution preserve the same output as unconditional recomputation while enforcing zero false-negative invalidation under the declared dependency contract?

### RQ3 — Software equivalence

Can TWRF and an independently managed software incremental runtime perform the same semantic work under identical mutation events?

### RQ4 — Architectural cost

Under what timing, region-granularity, dependency, and State Store assumptions does TWRF's hardware-oriented scheduling cost fall below equivalent software incremental management?

### RQ5 — Generality

Does one persistent execution contract remain useful across raster, ray, neural, and heterogeneous dependency graphs?

## Contributions

The defensible contributions of the current implementation are:

1. A precise persistent spatial work-object model, including lifecycle, version, dependency, state, and output semantics.
2. A runtime dependency-audit contract and failure semantics that make semantic assumptions testable.
3. An executable software incremental Baseline C using the same TWR graph, kernels, events, and persistent-state semantics as the architectural scheduler.
4. A virtual-GPU experimental methodology that separates measured simulator operations from derived timing estimates.
5. A reproducible evaluation showing both where the current TWRF model saves recomputation work and where management overhead dominates it.

The project does not claim that incremental computation, memoization, dependency tracking, persistent rendering resources, spatial tiling, or incremental path-traced rendering are individually novel.

## Experimental method

The comparison uses:

- Baseline A: unconditional full recomputation;
- Baseline B: temporal-cache abstraction;
- Baseline C: executable software incremental runtime;
- TWRF: architectural scheduler with persistent TWR semantics.

The standard matrix varies requested object mutation across:

{0, 0.05, 0.10, 0.25, 0.50, 0.75, 1.0}

and evaluates both clustered and dispersed locality.

The measured execution variables are:

p_o = mutated objects / objects

p_r = dirty TWRs / TWRs

p_e = executed TWRs / TWRs

Timing comparisons use p_e as the relevant recomputation variable.

## Acceptance gates

The result is considered interpretable only after:

semantic lifecycle correctness
-> dependency soundness
-> full-recompute oracle equivalence
-> TWRF/B3 execution-set parity
-> TWRF/B3 output parity
-> measured simulator accounting
-> parameterized timing
-> sensitivity analysis.

## Current result

The final validated raster campaign has:

- 14 matrix cases;
- 35 registered CTest tests;
- 14/14 TWRF-B3 execution-set parity;
- 14/14 TWRF-B3 bitwise output parity;
- 14/14 matrix dependency-audit success;
- 14/14 oracle parity on the dedicated full-matrix oracle test.

The default derived timing model currently yields:

- TWRF < full recompute only at p_e=0;
- TWRF > the temporal-cache model at every tested matrix point;
- TWRF < executable software incremental Baseline C at every tested matrix point;
- TWRF/B3 derived cost ratio between 1.634 and 2.400;
- no additional modeled reduction is required to cross below B3 at the tested points; the default model already places TWRF below B3.
- the software experiment runner now includes a 54-setting sensitivity grid over tile size, control-plane cost, and State Store latency.

These are simulator-derived results, not physical GPU measurements.

## Research conclusion

The current evidence supports TWRF as a coherent architecture and experimental framework. Under the declared timing model, it establishes a cost advantage over the executable software incremental Baseline C across the tested raster matrix and sensitivity range, but not over the temporal-cache abstraction or non-static full-recompute baseline. These are derived model results, not physical hardware performance measurements.

The strongest paper framing is therefore:

**TWRF is a persistent spatial execution architecture whose value is a measurable cost-boundary question.**

The sensitivity campaign is implemented as a 54-setting grid. It tests robustness of the TWRF cost model over declared region-size, control-plane, and State Store assumptions. It is not an unrestricted global optimization or proof of universal superiority.

## Threats to validity

The main threats are:

- explicit dependency observation is a runtime contract rather than automatic machine-code dependency inference;
- the ray workload is a bounded synthetic workload rather than a production path tracer;
- the neural workload is a small deterministic MLP;
- Baseline B is an abstraction, not a commercial cache implementation;
- Baseline C's cycle conversion is parameterized rather than a wall-clock benchmark;
- physical GPU and FPGA behavior remain unmeasured.

These are documented rather than hidden assumptions.
