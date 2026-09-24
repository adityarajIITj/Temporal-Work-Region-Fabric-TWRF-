# Temporal Work Region Fabric (TWRF) Architecture Specification

**Status:** Final research-branch specification  
**Version:** 1.0-research

## 1. Research hypothesis

TWRF investigates whether GPU work can be represented as persistent spatial computational objects whose valid outputs survive across frames and whose execution is driven by explicit input/dependency validity rather than unconditional frame-wide recomputation.

The hypothesis is conditional:

[
C_{TWRF}<C_{baseline}
]

only for workloads and architectural parameters for which the savings from avoiding recomputation exceed the cost of maintaining temporal state and scheduling metadata.

## 2. Temporal Work Region

A TWR is conceptually:

[
R_i=(ID_i,A_i,S_i,I_i,O_i,V_i,Sigma_i,D_i,Q_i)
]

where:

- (ID_i): persistent identity;
- (A_i): spatial extent;
- (S_i): persistent TWR state;
- (I_i): explicit external inputs;
- (O_i): persistent output;
- (V_i): recorded/current versions;
- (Sigma_i): change signature;
- (D_i): dependency relations;
- (Q_i): execution state.

The implementation uses a stable TWR ID across frames. A TWR may be IdleClean without executing in a frame.

## 3. Execution state machine

The implementation supports:

- IdleClean
- Dirty
- Ready
- Executing
- Failed

The valid success path is:

IdleClean -> Dirty -> Ready -> Executing -> IdleClean

Failure path:

Executing -> Failed -> Dirty (retryable)

A failed execution never publishes a valid new output version and never releases downstream readiness.

## 4. Input validity

External inputs are VersionedResource objects with monotonic versions.

A TWR is invalidated when a declared input version changes and its conservative spatial validity test requires recomputation.

For upstream outputs, the validity check uses the producer TWR's State Store output version.

The correctness rule is:

ObservedMutableDependencies(R_i) subset DeclaredDependencies(R_i).

## 5. Conservative invalidation

The implementation intentionally permits false positives:

[
FalsePositiveInvalidation Rightarrow Extra Work
]

but forbids false negatives:

[
FalseNegativeInvalidation Rightarrow Potentially Stale Output.
]

The desired optimization target is:

[
min FPI quad 	ext{subject to} quad FNI=0.
]

## 6. Dynamic dependency pruning

At frame preparation, the scheduler computes the active dirty set.

For a dirty TWR (T):

[
pending(T)=|{Pin D_{in}(T):Pin W}|
]

where (W) is the set of TWRs that will execute.

This means a clean upstream producer does not block its dirty consumer.

## 7. Persistent State Store

The LogicalStateStore holds persistent TWR outputs across frames and records:

- output version;
- payload bytes;
- last update step;
- allocated bytes;
- peak allocation;
- read/write counts;
- read/write byte traffic;
- configurable capacity.

A rejected capacity write does not create a phantom state slot and does not advance the TWR output version.

## 8. Scheduling

The architectural scheduler uses a priority queue with deterministic ordering:

1. higher priority;
2. greater topological depth;
3. lower TWR ID.

Baseline C uses a software ready-set scan with the same semantic ordering.

This permits direct execution-set parity testing.

## 9. Heterogeneous execution

The common execution object is used for:

### Raster
Screen-space tile TWRs. The current example uses 16x16 tiles and persistent color/depth outputs.

### Ray
Bounded ray batches. Camera, geometry, and light are versioned inputs.

Camera mutation regenerates derived primary rays before reuse:

[
Camera_tightarrow PrimaryRays_tightarrow RayTrace_t.
]

### Neural
Small deterministic MLP denoising blocks with persistent temporal output state.

### Heterogeneous DAG
The current representative graph is:

[
Rasterightarrow Ray
]

[
Rasterightarrow Neural
]

[
Rayightarrow Neural.
]

## 10. Dependency audit

Audited kernels report mutable dependencies explicitly using resource and upstream-producer observation contracts.

An execution fails the audit when:

[
Observed
otsubseteq Declared.
]

The audit is a runtime semantic contract; it does not instrument arbitrary hidden C++ loads automatically.

## 11. Baselines

### Baseline A
Unconditional full recomputation.

### Baseline B
Temporal cache abstraction with lookup and refill overhead.

### Baseline C
Executable software incremental scheduler using the same semantic graph, kernels, mutation trace, and persistent state model.

The research comparison therefore asks whether TWRF's architecture-oriented management can improve on equivalent software incremental management, not simply whether incremental reuse beats full recomputation.

## 12. Timing model

Measured simulator operation counts are converted to derived cycle values:

[
C_{model}=sum_j n_jc_j.
]

Control-plane counts include version checks, bounding checks, queue operations, dependency traversals, and failed executions. State Store traffic is accounted from measured byte counts.

Cycle parameters are configurable and do not represent measured silicon.

## 13. Experimental variables

The standard sweep reports:

[
p_o=\frac{mutated objects}{objects},
quad
p_r=\frac{dirty regions}{regions},
quad
p_e=\frac{executed regions}{regions}.
]

The requested change-rate sweep is:

[
p_oin{0,.05,.10,.25,.50,.75,1}
]

under clustered and dispersed mutation locality.

Additional experiments should vary region granularity, State Store capacity, dependency depth, and timing assumptions.

## 14. Oracle and parity requirements

Raster incremental output must match the forced recompute oracle.

The TWRF/B3 pair must satisfy:

[
E_{TWRF}=E_{B3}
]

and:

[
Output_{TWRF}=Output_{B3}.
]

Only after these gates pass should modeled cycle comparisons be interpreted.

## 15. Claim boundary

TWRF does not claim individual novelty for:

- incremental computation;
- dependency tracking;
- memoization;
- persistent graphics resources;
- spatial tiling;
- incremental path-traced rendering.

The research contribution is the architectural combination of these concepts into a persistent spatial execution object and the empirical/derived evaluation of its management cost.

## 16. Non-goals of the current simulator

The simulator does not claim to model:

- production GPU firmware;
- real driver launch latency;
- vendor cache hierarchies;
- physical SRAM timing;
- full stochastic multi-bounce path tracing;
- a production-scale neural accelerator;
- synthesized FPGA resource/timing closure.

Those are future hardware-validation stages.
