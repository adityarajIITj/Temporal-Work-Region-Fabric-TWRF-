# TWRF Research Validation Gate

**Status:** Final validation specification for the research branch  
**Purpose:** Define the minimum evidence required before a TWRF architectural conclusion is treated as supported.

## Gate 0 — Build and lifecycle validity

Acceptance:

- project configures and builds cleanly;
- all registered regression tests pass;
- TWR lifecycle transitions do not mark failed work clean;
- failed State Store commits do not advance output versions;
- failed producers do not release downstream consumers.

## Gate 1 — Dependency soundness

The required invariant is:

[
ObservedMutableDependencies(T)
subseteq
DeclaredDependencies(T)
]

The runtime audit covers:

- versioned external resources;
- producer-output dependencies.

The following must hold for the built-in workloads:

[
AuditFailures=0.
]

Important scope limitation: the audit is an explicit runtime contract. It does not automatically instrument arbitrary C++ memory reads through captured objects.

## Gate 2 — Full-recompute oracle

For raster workloads, incremental rendering must be compared with a forced full-recompute oracle on the same post-mutation scene.

Required output condition:

[
Output_{incremental}=Output_{full}.
]

The current renderer exposes bitwise framebuffer comparison and pixel-difference helpers.

## Gate 3 — Software incremental parity

Baseline C is an executable software incremental scheduler using:

- the same graph;
- the same TWR kernels;
- the same mutation event;
- the same persistent-state semantics;
- the same deterministic ordering rule.

For each paired experiment:

[
E_{TWRF}=E_{B3}.
]

The final matrix requires both execution-set parity and bitwise output parity.

## Gate 4 — Measured simulator accounting

The simulator records operation counts separately from cycle estimates.

Required measurable quantities include:

| Counter | Meaning |
|---|---|
| resource_version_checks | external resource version comparisons |
| producer_version_checks | producer output-version comparisons |
| bounding_checks | spatial checks after a detected version change |
| dirty_propagations | downstream invalidation events |
| dependency_traversals | successful producer-to-consumer notifications |
| ready_queue_pushes | scheduler ready-set insertions |
| ready_queue_pops | scheduler dispatch removals |
| failed_executions | unsuccessful TWR execution attempts |
| State Store bytes | actual measured read/write traffic |

These counters must be the source of the corresponding control-plane timing terms.

## Gate 5 — Parameterized timing model

Cycle values are derived:

[
C_{model}=sum_j n_j c_j
]

where (n_j) is a measured simulator operation count and (c_j) is an explicitly declared timing parameter.

No parameterized cycle result may be called a physical-GPU measurement.

## Gate 6 — Workload matrix

The standard campaign contains:

[
p_oin{0,.05,.10,.25,.50,.75,1}
]

under at least:

- clustered locality;
- dispersed locality.

The experiment separately reports:

[
p_o=\frac{mutated objects}{objects},
quad
p_r=\frac{dirty TWRs}{TWRs},
quad
p_e=\frac{executed TWRs}{TWRs}.
]

The simple analytical break-even relation is expressed using (p_e), not (p_o):

[
C_{TWRF}=C_t+p_eC_r
]

and:

[
p_e<1-\frac{C_t}{C_r}.
]

## Gate 7 — Baseline comparison

Three reference models are required:

**A — Full recomputation**

[
C_A=C_r
]

**B — Temporal cache**

Cache lookup, validation, miss/refill, and eviction overheads are modeled explicitly.

**C — Software incremental runtime**

Measured software control-plane operations are converted through software timing parameters.

The principal architectural question is:

[
C_{TWRF}<C_{B3} ?
]

A TWRF advantage over Baseline A alone is insufficient to establish a hardware-architecture advantage, because software incremental computation may capture much of the same semantic reuse.

## Gate 8 — Sensitivity analysis

The implementation now provides a reproducible 54-setting sensitivity grid:

- tile sizes: {8, 16, 32};
- hardware control-plane multipliers: {0.25, 0.50, 0.75, 1.00, 1.50, 2.00};
- State Store latency multipliers: {0.50, 1.00, 2.00}.

Each setting evaluates the complete 14-case mutation/locality matrix and preserves the same semantic workload and parity gates. The generated artifact is `results/twrf_sensitivity.json`.

The objective is not to select one favorable point but to characterize the parameter boundary at which management overhead dominates computation savings. Any reported region in which TWRF is below Baseline C must still retain execution-set parity, output parity, zero dependency-audit failures, full-recompute correctness, and the declared timing assumptions.

## Gate 9 — Hardware realization boundary

FPGA/RTL material is a feasibility study until:

1. synthesizable RTL exists;
2. the target device is specified;
3. synthesis results are produced;
4. timing closure is measured;
5. simulator traces are replayed against RTL.

Estimated LUT/BRAM/DSP figures must remain labeled estimates before those steps.

## Final acceptance statement

The research is ready for thesis/paper drafting when all implemented semantic and parity gates pass and the experimental results are clearly separated into:

1. measured simulator quantities;
2. derived timing-model quantities;
3. hardware feasibility estimates.

The final conclusion must be conditional on workload and architecture parameters.
