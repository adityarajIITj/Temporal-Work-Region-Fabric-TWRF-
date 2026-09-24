# TWRF Execution Semantics and Formal Invariants

**Status:** Final semantic contract for the research implementation.

## 1. Temporal Work Region state

A TWR is represented conceptually as:

R_i=(ID_i,A_i,S_i,I_i,O_i,V_i,Sigma_i,D_i,Q_i)

where ID_i persists across frames even when the TWR is clean and does not execute.

The implemented lifecycle is:

Dirty -> Ready -> Executing -> IdleClean

with an explicit failure state:

Executing -> Failed.

A failed execution may be retried, but it does not publish a valid output or release downstream consumers.

## 2. Clean-state invariant

A TWR can become clean only after:

1. its kernel reports success;
2. dependency auditing, when enabled, passes;
3. its output commit succeeds;
4. current resource versions are recorded;
5. upstream output versions are recorded.

Therefore a failed kernel or failed output commit cannot silently become reusable.

## 3. Dependency soundness

For every executable TWR:

ObservedMutableDependencies(R_i) subset DeclaredDependencies(R_i).

Observed external resources are reported with the observe_resource(ResourceId) contract, and observed producer outputs with the observe_upstream_producer(TWRId) contract.

The runtime audit rejects an execution if an observed dependency is absent from the TWR declaration.

This is intentionally a contract checker rather than automatic compiler-level memory instrumentation.

## 4. Invalidation asymmetry

False positives are safe:

FalsePositiveInvalidation -> Extra Work

False negatives are unsafe:

FalseNegativeInvalidation -> Potentially Stale Output.

The acceptance condition is:

FNI=0.

Optimization therefore becomes:

min FPI subject to FNI=0.

## 5. Version validity

For each bound external resource r, the TWR stores a recorded version v_r.

A resource change is detected when V(r) != v_r.

For spatial resources the implementation may then apply a conservative region filter. Bounding tests are performed after a version change rather than unconditionally.

For an upstream TWR P, the analogous validity condition uses its persistent State Store output version.

## 6. Dependency readiness

A dirty TWR with no dirty upstream producer is eligible for the ready set immediately.

For a dirty TWR T:

pending(T) = |{P in D_in(T): P will execute this frame}|.

This avoids waiting for clean producers that intentionally emit no completion event.

A downstream TWR becomes eligible only after every active upstream producer has successfully committed.

## 7. Deterministic scheduling

The TWRF priority queue orders ready work by:

1. higher priority;
2. greater topological depth;
3. lower TWR ID.

The software Baseline C scheduler implements the same semantic ordering with a linear ready-set scan. This allows execution-set parity to be tested without requiring the software baseline to use the same hardware-oriented data structure.

## 8. Persistent state

A TWR's output remains in the State Store across frames.

The output version advances only after successful execution and commit:

V_O^(t+1) = V_O^t + 1.

A skipped TWR retains its previously committed output.

This persistence is the architectural mechanism that permits temporal reuse.

## 9. Derived-input freshness

Derived state must be refreshed whenever its generating input changes.

The Ray workload explicitly maintains:

Camera_t -> PrimaryRays_t -> RayTrace_t.

A camera mutation regenerates primary rays before ray-batch execution can consume them.

This prevents the invalid condition in which an invalidation event occurs but the kernel still consumes a stale derived representation.

## 10. Oracle equivalence

For raster workloads, the incremental output is compared with unconditional full recomputation on the same mutated scene:

Output_TWRF = Output_Full.

The renderer provides bitwise framebuffer comparison.

## 11. Software baseline equivalence

For paired TWRF/B3 experiments:

E_TWRF(t) = E_B3(t)

and:

Output_TWRF(t) = Output_B3(t).

Only after these conditions hold should cycle-model comparisons be interpreted as comparisons of architecture rather than differences in semantic work performed.

## 12. Heterogeneous dependency contract

The representative heterogeneous DAG contains:

Raster -> Ray,
Raster -> Neural,
Ray -> Neural.

Neural processing also consumes its own previously committed temporal output as persistent state.

This validates that TWR semantics are not restricted to one compute domain.

## 13. Correctness hierarchy

The implementation therefore establishes a hierarchy of claims:

Dependency Soundness
-> Invalidation Safety
-> Execution Correctness
-> Workset Parity
-> Performance Comparability.

Each stage is a prerequisite for the next.
