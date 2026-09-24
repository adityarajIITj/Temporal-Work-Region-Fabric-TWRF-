# Architecture Decision Records — TWRF

**Status:** Final research-branch decision log.

## ADR-001: Integer Versioning over Buffer Hashing

TWRF uses monotonic version numbers on explicit VersionedResource objects rather than hashing large buffers to discover change. Spatial checks occur only after a relevant version change.

This is an architectural cost choice, not a theorem that hashing is never useful.

## ADR-002: Logical State Store

The State Store is modeled as a configurable logical persistent memory system. Capacity, allocation, and read/write timing are parameters until a physical implementation exists.

No fixed SRAM size in this repository should be interpreted as a measured hardware specification.

## ADR-003: Deterministic Ready Ordering

The architectural scheduler orders ready TWRs by:

1. priority, descending;
2. topological depth, descending;
3. TWR ID, ascending.

Baseline C implements the same semantic ordering with a linear software ready set.

## ADR-004: Dynamic Dependency Pruning

A dirty consumer waits only for upstream TWRs that are also scheduled to execute in the current frame.

Clean producers already have committed valid state and must not be treated as unfinished work.

## ADR-005: Unified Heterogeneous TWR Contract

Raster, ray, and neural workload stages are represented by the same TWR metadata, lifecycle, State Store, and dependency machinery.

This isolates the effect of the execution model from the existence of separate specialized functional blocks.

## ADR-006: State Store Read Accounting

State Store read_output() is const-qualified while its internal traffic counters remain mutable. This permits read-only inspection without const_cast.

The implementation does not claim that those mutable counters are a thread-safety mechanism; concurrent synchronization is outside the current simulator contract.

## ADR-007: Conservative Spatial Invalidation

Spatial overlap is a conservative filter. False-positive invalidation is acceptable; false-negative invalidation is not.

The optimization target is:

min FPI subject to FNI=0.

## ADR-008: Explicit Dependency Audit

The simulator exposes a runtime dependency-audit contract through resource and producer observation calls.

An audited execution fails if it observes a mutable dependency that is absent from its declaration.

This is a runtime contract checker, not automatic machine-code load instrumentation.

## ADR-009: Executable Software Incremental Baseline

Baseline C is an executable software incremental scheduler using the same graph, kernels, mutation events, and persistent-state semantics as TWRF.

The difference is management placement:

- TWRF: architecture-oriented scheduler and ready queue;
- B3: explicit software scanning and ready-set selection.

Cycle results remain derived from measured operation counts and timing parameters.

## ADR-010: Derived Ray Input Refresh

Camera changes invalidate the ray workload and regenerate primary rays before reuse.

This establishes the dependency chain:

Camera -> PrimaryRays -> RayTrace.

Without this refresh, resource invalidation could trigger correct re-execution of an incorrect stale derived input.

## ADR-011: Evidence Separation

Every result is classified as one of:

1. measured simulator quantity;
2. derived timing-model quantity;
3. hardware feasibility estimate.

Only the first category is a direct simulator measurement.

## ADR-012: Prior-Art Claim Boundary

TWRF does not claim individual novelty for incremental computation, dependency tracking, memoization, persistent graphics resources, spatial tiling, or incremental path-traced rendering.

The research contribution under study is the GPU-oriented combination of persistent spatial work identity, state/output, validity, dependencies, and scheduling.

## ADR-013: Final Acceptance Principle

A performance comparison is interpretable only after:

semantic correctness -> dependency soundness -> oracle equivalence -> B3 parity -> measured accounting -> timing analysis.

This decision prevents a favorable timing model from compensating for a semantic mismatch.
