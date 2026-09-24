# TWRF Research Claim Matrix

This document freezes the claims that may be made from the current repository and distinguishes implementation evidence from modeled and prospective evidence.

| Claim | Evidence class | Current support | Permitted wording | Not permitted |
|---|---|---|---|---|
| TWR is a persistent spatial work object with stable identity, state/output, versions, dependencies, and execution state | Implemented | TWR core model and lifecycle | "The simulator implements..." | "This abstraction is unprecedented" |
| Failed work cannot become clean | Implemented | Lifecycle regression tests | "Failure is explicitly represented and excluded from clean state" | "The implementation is production fault-tolerant" |
| State Store commit failure prevents a valid TWR commit | Implemented | Capacity-failure regression test plus rollback semantics | "Commit success is part of execution validity" | "The design is transactionally complete for arbitrary kernels" |
| Declared resource/producer dependencies can be checked against explicit observations | Implemented | Dependency audit tests and workload instrumentation | "The simulator provides an opt-in runtime dependency-audit contract" | "The compiler automatically discovers all dependencies" |
| The tested raster workload has no audit failures | Measured simulator result | 14-case matrix documented as zero failures; CI is configured to verify generated results | "In the documented matrix, zero dependency-audit failures were observed" | "All possible TWR programs are dependency-safe" |
| Incremental raster output equals full recomputation for the tested matrix | Measured simulator result | Dedicated 14-case oracle test | "The tested matrix passes bitwise oracle comparison" | "TWRF guarantees universal bitwise equivalence" |
| TWRF and Baseline C perform the same semantic work under the tested matrix | Measured simulator result | Execution-set and output parity gates | "The tested matrix demonstrates execution/output parity" | "Hardware and software are identical implementations" |
| Baseline C is an executable software incremental runtime | Implemented | SoftwareIncrementalScheduler | "Baseline C is executable and uses the same graph, kernels, events, and persistent-state semantics" | "Baseline C is a benchmark of a commercial incremental renderer" |
| TWRF can reduce recomputation work when regions remain valid | Measured simulator behavior | Static and sparse-change tests | "The simulator demonstrates recomputation avoidance in the tested workloads" | "Temporal coherence always improves performance" |
| Default timing model shows TWRF below full recomputation only in the static case | Derived timing result | docs/RESULTS.md | "Under the declared default model..." | "TWRF is faster on GPUs" |
| Default timing model places TWRF below Baseline C in all 14 documented matrix cases | Derived timing result | CI-generated matrix | "Under the declared default timing parameters..." | "This proves physical hardware superiority" |
| The 135-setting sensitivity grid tests robustness of TWRF cost assumptions over a declared parameter range | Derived timing experiment | Implemented generator and CI schema checks | "The experiment sweeps declared architectural parameters" | "The sensitivity study proves the full cost boundary or optimal hardware parameters" |
| FPGA mappings are feasible enough to motivate future RTL work | Estimated | docs/FPGA_FEASIBILITY.md | "Illustrative feasibility estimates..." | "The FPGA design was synthesized/timed" |
| TWRF is a new combination worth investigating | Research hypothesis | Architecture + prior-art boundary | "TWRF investigates a GPU-oriented combination of..." | "TWRF is the first/only/novel system of its kind" |
| TWRF beats a physical GPU baseline | Unsupported | No physical GPU benchmark | None | Any FPS/speedup/energy claim |
| TWRF saves energy or power | Unsupported | No silicon/board measurements | None | Any energy-efficiency claim |
| TWRF scales to production path tracing or large neural renderers | Unsupported | Small representative workloads only | None | Production-scale generalization |

## Evidence vocabulary

Use **implemented** for behavior directly present in the source and regression tests.

Use **measured simulator result** for counters, traces, parity checks, and State Store activity observed by executing the simulator.

Use **derived timing result** for cycle values obtained by applying declared timing parameters to measured simulator operation counts.

Use **estimated** for FPGA/RTL resource and timing projections that have not been synthesized or measured.

Use **hypothesis** for architectural claims that require hardware validation.

## Publication rule

Every performance statement in a paper or presentation should identify whether it is measured, derived, or estimated. A cycle count from the timing model must never be described as a physical GPU measurement.

## Current research position

The strongest supportable conclusion is that TWRF is a coherent, reproducible virtual-GPU architecture study of persistent spatial work objects and that its economic value is a parameter- and workload-dependent question. The current software evidence establishes semantics, dependency auditing, oracle parity, software-baseline parity, and a parameterized cost framework. It supports a derived model advantage over the executable software incremental baseline under the tested assumptions, but it does not establish physical GPU performance.
