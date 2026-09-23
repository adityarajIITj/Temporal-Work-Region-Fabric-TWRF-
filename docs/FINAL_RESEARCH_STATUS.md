# TWRF Final Research Status

## Scope

This document freezes the research position of the Temporal Work Region Fabric (TWRF) repository at the completion of the semantic-hardening and comparative-baseline phase.

## Architecture under study

TWRF represents computation as persistent spatial work objects:

R_i = (ID_i, A_i, S_i, I_i, O_i, V_i, Sigma_i, D_i, Q_i)

The defining architectural combination is:

Persistent Spatial Work Identity
+ Persistent State
+ Version/Validity Tracking
+ Explicit Dependencies
+ Hardware-Oriented Scheduling

This is an architectural research hypothesis, not a claim that each constituent mechanism is individually new.

## Implemented evidence

The repository currently contains:

- explicit TWR lifecycle including Failed state;
- success-gated State Store commits;
- failure-safe dependency propagation;
- runtime dependency auditing for explicit resource and producer observations;
- persistent State Store versioning and traffic metrics;
- deterministic architectural scheduling;
- an executable software incremental Baseline C;
- frame-scoped execution-set parity checks between TWRF and B3;
- raster full-recompute oracle comparison;
- derived-ray refresh after camera mutation;
- heterogeneous Raster -> Ray, Raster -> Neural, and Ray -> Neural dependencies;
- parameterized A/B/C/TWRF timing models;
- a 54-setting timing sensitivity grid over region size, control-plane cost, and State Store latency;
- machine-readable sensitivity export and CI validation;
- measured control-plane operation counters;
- 7 x 2 mutation/locality experimental matrix generation;
- machine-readable experiment output and analysis plotting;
- CI build, regression testing, demo execution, and JSON validation.

## Correctness contract

The principal dependency invariant is:

ObservedMutableDependencies(R_i) subset DeclaredDependencies(R_i)

The false-negative invalidation target is:

FNI = 0

False-positive invalidation is treated as an efficiency issue.

A TWR becomes clean only following successful computation, successful audit when enabled, and successful State Store commit.

## Baseline C

Baseline C is executable rather than merely symbolic. It uses the same semantic graph, kernels, mutation event, persistent-state representation, and deterministic ordering semantics while replacing the architectural ready queue with explicit software scanning and a software ready set.

The comparison is therefore intended to answer:

Does hardware-oriented work-region management reduce management cost relative to equivalent software incremental management?

Cycle results remain derived from measured simulator operation counts and explicit timing parameters.

## Experimental variables

The repository distinguishes:

p_o = mutated objects / objects

p_r = dirty TWRs / TWRs

p_e = executed TWRs / TWRs

The simplified break-even condition is:

p_e < 1 - C_t / C_r

The complete model contains separate control-plane, execution, State Store, scene-memory, and interconnect terms.

## Evidence categories

### Measured

- executions;
- skips;
- version checks;
- bounding checks;
- dependency traversals/propagations;
- ready-set/queue operations;
- failed executions;
- State Store operation counts;
- State Store bytes;
- execution traces;
- output/workset parity;
- dependency-audit outcomes.

### Derived

- cycle totals generated from measured operation counts and configurable timing parameters;
- analytical break-even thresholds.

### Estimated

- FPGA/RTL resource mappings before synthesis;
- prospective hardware timing and utilization before physical validation.

These categories must not be conflated in a paper or presentation.

## Prior-art boundary

The research does not claim individual novelty for incremental computation, dependency tracking, memoization, persistent graphics resources, spatial tiling, or incremental path-traced rendering.

The defensible claim is that TWRF investigates the particular GPU-oriented combination and its architectural cost/benefit boundary.

## Current experimental finding

The validated 14-case raster matrix has full execution-set parity, full output parity, and zero dependency-audit failures. Under the current default timing parameters, the derived model does not show TWRF beating Baseline C or the temporal-cache model across the tested mutation/locality cases. TWRF is below the full-recompute model only for the completely static case. See docs/RESULTS.md for the complete matrix and interpretation.

## Remaining external validation

The following are outside the current software-simulator evidence boundary:

- physical GPU benchmarking;
- production driver overhead;
- real vendor cache behavior;
- FPGA synthesis and timing closure;
- silicon power/energy measurements;
- production-scale path tracing and neural workloads.

These are future validation stages, not missing software-test cases.

## Final publication-safe statement

TWRF provides a reproducible virtual-GPU study of persistent spatial work objects with explicit validity, dependency, state, and scheduling semantics. It establishes semantic and software-baseline validation in simulation and provides a parameterized architectural timing framework for studying when persistent work management can offset recomputation cost. Claims about physical GPU speed, energy efficiency, or universal superiority require separate hardware evidence.
