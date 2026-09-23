# TWRF Limitations, Threats to Validity, and Required Interpretations

TWRF is a research architecture simulator. This document separates implemented behavior, modeled behavior, and assumptions that still require external validation.

## 1. No physical-GPU performance claim

The simulator produces measured simulator operation counts and derived cycle estimates. It does not measure an NVIDIA, AMD, Apple, Intel, ARM, PowerVR, or other physical GPU.

Consequently:

- modeled "cycles" are not hardware benchmark results;
- a modeled speedup is not an FPS or energy measurement;
- FPGA figures are estimates until synthesized and measured.

## 2. Incremental benefit is workload-dependent

The basic single-region model is:

[
C_{TWRF}=C_t+p_eC_r
]

versus:

[
C_{full}=C_r
]

so the simplified threshold is:

[
p_e < 1-rac{C_t}{C_r}.
]

This is an analytical condition, not a universal empirical threshold.

The actual implementation has additional terms for State Store traffic, scene memory, interconnect movement, dependency management, and region granularity. The full experiment therefore reports measured (p_o), (p_r), and (p_e) separately.

## 3. Object mutation is not region mutation

The workload generator's input parameter controls the fraction of objects intentionally mutated:

[
p_o=rac{mutated objects}{objects}.
]

That is not the same quantity as the fraction of dirty regions:

[
p_r=rac{dirty TWRs}{TWRs}
]

or the executed fraction:

[
p_e=rac{executed TWRs}{TWRs}.
]

The experiment output records all three. Claims about the volatility of the execution fabric should use (p_e) or (p_r), not silently reinterpret (p_o).

## 4. Conservative invalidation can create false positives

Raster TWRs currently bind broad resource sets and use conservative old/new screen-bound unions. This is designed to protect correctness rather than minimize invalidation.

Therefore the architecture may re-execute regions whose final pixels did not actually change:

[
FPI>0
]

is expected in some workloads.

The research target is:

[
min FPI quad 	ext{subject to} quad FNI=0.
]

A future optimized implementation should investigate hierarchical spatial indexing, finer resource-to-region mappings, and dependency-specific invalidation.

## 5. Dependency auditing is explicit, not automatic program analysis

The implemented dependency audit checks the resources and producer outputs that a workload explicitly reports through:

- observe_resource()
- observe_upstream_producer()

It is therefore a runtime contract checker, not a compiler or CPU/MMU mechanism that can transparently intercept every C++ load.

Hidden mutable state accessed through arbitrary captured objects can escape the audit unless the kernel explicitly reports it.

This limitation must remain explicit in the thesis.

## 6. B3 is an executable semantic baseline, but its timing is modeled

Baseline C now has a real software incremental scheduler:

- explicit resource-version scanning;
- explicit producer-version scanning;
- explicit dirty propagation;
- software ready-set management;
- the same TWR kernels and persistent state semantics as TWRF.

The comparison still uses a parameterized software timing model. The model assigns costs to measured B3 operations; it is not a wall-clock benchmark of a production incremental rendering engine.

## 7. CPU/GPU partitioning is abstracted

The simulator abstracts application-side mutation events and the hardware/software boundary.

It does not yet model:

- kernel launch latency from a real driver;
- PCIe or CXL transfer costs;
- actual GPU firmware scheduling;
- real cache coherence protocols;
- page tables or virtual-memory faults;
- multi-GPU synchronization;
- operating-system preemption.

These are outside the present research scope but matter to a physical implementation.

## 8. Ray workload scope

The ray example is intentionally small. It uses bounded primary-ray batches and shadow visibility rather than a complete path-tracing implementation.

A previous semantic hazard was identified and corrected: moving the camera now regenerates derived primary rays before the ray batches are reused.

The model still does not cover the full dependency complexity of multi-bounce global illumination, reservoir-based sampling, or stochastic ray histories.

## 9. Neural workload scope

The neural example uses a deterministic small MLP. It is a workload representative, not a claim that TWRF implements a production neural renderer.

Persistent temporal state is modeled through the previous neural output stored in the State Store.

## 10. Region granularity trade-off

Smaller TWRs improve spatial selectivity but increase:

- metadata storage;
- version-check fan-out;
- queue pressure;
- dependency bookkeeping;
- State Store metadata and access overhead.

Larger regions reduce management cost but increase false-positive invalidation.

Therefore there is an architecture-dependent optimum region size rather than a universally optimal tile size.

## 11. State Store pressure

The current State Store supports configurable capacity limits and accounts for rejected writes. A physical architecture would need an explicit policy for:

- replacement;
- spilling;
- prioritization;
- recomputation versus reload;
- bandwidth contention.

Until such a policy is implemented and measured, claims about very large persistent working sets remain architectural hypotheses.

## 12. Heterogeneous DAG scope

The heterogeneous pipeline validates the common TWR contract across raster, ray, and neural stages. It does not establish the scalability of one shared scheduler to a production-sized GPU dependency graph.

The current DAG is intentionally small so the causal structure is inspectable.

## 13. Determinism scope

The simulator defines deterministic TWR selection and uses deterministic kernels. This supports reproducible simulation traces.

It does not prove bitwise determinism for arbitrary future hardware implementations with relaxed floating-point modes, parallel reductions, or vendor-specific math units.

## 14. Prior-art threat

Incremental computation has a deep prior-art literature, including self-adjusting computation, named incremental computation, persistent render-graph resources, and incremental path-traced rendering.

TWRF's research claim is therefore intentionally narrower: it evaluates a GPU-oriented architectural combination of persistent spatial work identity, state/output, validity, dependencies, and scheduling.

No individual mechanism should be presented as unprecedented.

## 15. Final interpretation rule

A convincing TWRF result requires the following order of evidence:

[
Semantic correctness
ightarrow
Dependency soundness
ightarrow
Oracle equivalence
ightarrow
B3 parity
ightarrow
Measured simulator accounting
ightarrow
Parameterized timing
ightarrow
Sensitivity
ightarrow
Hardware study.
]

Performance conclusions that bypass these gates should not be treated as final research evidence.
