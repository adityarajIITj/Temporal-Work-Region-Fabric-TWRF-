# TWRF Research Validation Gate

This document defines the validation sequence required before using TWRF performance results as architectural evidence.

## Gate 0 — Semantic validity

A Temporal Work Region (TWR) is valid only when its output is committed successfully and its recorded input/dependency versions describe the computation that produced that output.

Required invariants:

- Failed execution never becomes IdleClean.
- Failed producer execution never releases downstream consumers.
- State Store commit failure is execution failure.
- Output version advances only after successful computation and output commit.
- A rejected State Store write does not allocate a phantom slot.
- Every direct mutable producer/output consumed by a TWR is represented in the dependency graph.

## Gate 1 — Dependency soundness

ObservedMutableDependencies(R_i) must be a subset of DeclaredDependencies(R_i).

False-negative invalidation must be zero:

FNI = missed required invalidations / required invalidations = 0

False positives may exist and should be measured separately.

## Gate 2 — Full recomputation oracle

For every workload/event trace, compare incremental execution against forced recomputation.

Required:

Output_TWRF(t) == Output_Full(t)

for all tested frames and all externally observable outputs.

The equality criterion must be workload-specific: exact byte equality where deterministic, otherwise an explicitly documented numerical tolerance.

## Gate 3 — Software incremental parity (B3)

B3 is the software-equivalent incremental runtime. It must use the same:

- TWR partition
- resource/version events
- dependency DAG
- mutation trace
- persistent-state semantics
- correctness oracle.

Required execution-set parity:

Executed_B3(t) == Executed_TWRF(t)

before comparing cost.

B3 interface:

- register_region(id, spatial_extent, inputs, dependencies, state_descriptor)
- update_resource(resource_id, new_version)
- mark_dirty(region_id, reason)
- propagate_invalidations()
- ready_regions()
- execute(region_id)
- commit(region_id, output)
- frame_trace()

B3 should not receive privileged knowledge unavailable to TWRF. It represents the same semantic mechanism in software; only placement of management work differs.

## Gate 4 — Measured simulator accounting

Record separately:

- TWRs total
- TWR executions
- TWR skips
- execution fraction p_e = K/N
- dependency traversals
- version checks
- ready-queue operations
- State Store reads/writes
- bytes read/written
- false-positive invalidations
- failed executions
- retry count.

These are measured simulator quantities, not hardware cycle measurements.

## Gate 5 — Architectural timing model

Only after Gates 0–4 pass should modeled timing be evaluated.

For TWRF:

C_TWRF = C_detect + C_prop + C_schedule + C_state + C_graph + p_e C_r

For B3:

C_B3 = C_detect_SW + C_prop_SW + C_schedule_SW + C_state_SW + C_graph_SW + p_e C_r

The architectural question is not merely whether TWRF beats full recomputation. The stronger comparison is whether the hardware-oriented organization reduces incremental-management cost relative to an equivalent software incremental runtime.

## Gate 6 — Parameter sweeps

Sweep independently:

- object mutation fraction p_o
- dirty-region fraction p_r
- executed-region fraction p_e
- clustered vs dispersed changes
- tile/region granularity
- dependency depth
- persistent-state size
- State Store capacity.

Do not use object mutation rate as a substitute for executed TWR fraction.

## Acceptance criteria

A result set is admissible for architectural claims only if:

1. FNI = 0 for the tested dependency/invalidation space.
2. Incremental output matches the full-recompute oracle.
3. B3 and TWRF execute the same semantic work under the same event trace.
4. Failure and State Store commit semantics are tested.
5. Measured quantities and modeled cycles are reported separately.
6. Any advantage over B3 is attributable to explicit architectural mechanisms rather than unequal workload semantics.
7. Worst-case results are reported rather than omitted.

## Current P0 regression coverage

This branch adds regression coverage for:

- failed producer propagation
- State Store commit failure
- rejected-write phantom slot creation
- direct Raster-to-Neural dependency declaration.

The remaining major validation item is dependency-read auditing: the simulator currently permits kernels to capture application state directly, so declared resource bindings are not yet a complete proof of the actual mutable read set.
