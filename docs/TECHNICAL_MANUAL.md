# TWRF Technical Manual

Status: Current research-branch methodology and implementation guide.

## 1. Purpose

Temporal Work Region Fabric (TWRF) is a virtual-GPU research architecture for studying persistent, spatially bounded computation across frames.

The project is not a physical GPU implementation. Its purpose is to test a specific execution model and quantify its management trade-offs in a controlled simulator.

## 2. Research question

The primary question is:

Can a GPU-oriented representation of persistent spatial work reduce the cost of incremental execution relative to both:

1. unconditional recomputation; and
2. an equivalent software-managed incremental runtime?

The second comparison matters because incremental computation itself is established prior art.

The simplified cost relation is:

C_TWRF = C_t + p_e C_r

and the simplified break-even condition is:

p_e < 1 - C_t / C_r.

The full simulator includes additional execution, memory, State Store, dependency, scheduling, and interconnect terms.

## 3. Temporal Work Region

A TWR is a persistent object with:

- stable identity;
- spatial extent;
- persistent State Store output/state;
- explicit versioned inputs;
- dependency edges;
- execution state;
- output version.

Its identity persists even when it skips execution for many frames.

The research abstraction is:

R_i = (ID_i, A_i, S_i, I_i, O_i, V_i, Sigma_i, D_i, Q_i).

## 4. Lifecycle

Implemented states:

IdleClean
Dirty
Ready
Executing
Failed

Successful execution requires a successful kernel result, valid audited dependencies when auditing is enabled, and a successful State Store commit.

A failure does not advance the output version and does not release downstream dependency readiness.

## 5. Versioned invalidation

Every explicit mutable input has a monotonic version.

The change-tracking path first compares versions. Spatial intersection is evaluated after a relevant version change.

The conservative rule is asymmetric:

False positive -> extra work but still valid output.
False negative -> possible stale output and therefore unacceptable.

Target:

min FPI subject to FNI = 0.

## 6. Dependency graph

A TWR may consume:

- VersionedResource objects;
- upstream TWR outputs.

Consumers are connected explicitly through TWRGraph.

Frame preparation determines which dirty TWRs will actually execute and counts only those producers as pending dependencies. This prevents clean producers from blocking dirty consumers.

## 7. Dependency audit

Built-in workloads enable the runtime dependency audit.

Kernels explicitly report:

observe_resource(resource_id)

and:

observe_upstream_producer(twr_id)

The audit verifies:

ObservedMutableDependencies(T) subset DeclaredDependencies(T).

This catches undeclared dependencies that a kernel reports. It does not transparently inspect arbitrary C++ loads or infer dependencies from captured object state.

## 8. Persistent State Store

LogicalStateStore represents persistent architectural state.

It records:

- allocation;
- capacity;
- peak allocation;
- output versions;
- read/write operations;
- read/write bytes.

Capacity rejection is transactional with respect to slot creation: a rejected write cannot create a new phantom slot.

The State Store is a logical model; no claim of physical SRAM timing is made without hardware validation.

## 9. Scheduler

TWRScheduler uses a ready priority queue with deterministic ordering:

1. priority descending;
2. topological depth descending;
3. TWR ID ascending.

The same semantic ordering is implemented in Baseline C using a software ready-set scan.

This difference is intentional: the baseline represents alternative management placement, not a duplicate implementation of the same hardware data structure.

## 10. Baseline C software incremental runtime

SoftwareIncrementalScheduler is executable and uses the same TWR kernels and semantic graph as the TWRF path.

It explicitly performs:

- resource version scans;
- producer version scans;
- dirty propagation;
- software ready-set management;
- persistent State Store execution;
- deterministic selection.

The timing model converts its measured operation counts into software-side cycle estimates. Those estimates are not wall-clock measurements.

## 11. Raster workload

The raster example partitions a frame into persistent screen-space tiles.

The tile kernel:

1. reads explicit scene resources;
2. conservatively culls objects;
3. rasterizes relevant triangles;
4. commits color/depth output to State Store.

Mutation methods exist for camera, object transforms, and textures.

A full recompute oracle re-rasterizes every tile from the current scene and provides the reference framebuffer.

## 12. Ray workload

RayBatchPipeline uses bounded primary-ray batches.

Versioned resources cover camera, geometry, and light.

Camera changes regenerate derived primary rays before ray execution:

Camera -> PrimaryRays -> RayTrace.

The current ray implementation is intentionally smaller than a production path tracer. Its role is to test whether the same TWR lifecycle can manage a bounded ray workload.

## 13. Neural workload

TemporalMLPPipeline provides a deterministic MLP.

Weights and input are versioned resources. Persistent State Store output contains the data needed for temporal state progression.

The heterogeneous denoiser similarly consumes raster and ray outputs through explicit producer dependencies.

## 14. Heterogeneous DAG

The representative graph contains:

Raster -> Ray
Raster -> Neural
Ray -> Neural

The direct Raster -> Neural edge is required because the neural stage reads the raster G-buffer directly.

This tests a shared persistent scheduling contract across heterogeneous compute domains.

## 15. Measurement discipline

Every experiment separates:

### Measured simulator quantities

- TWR executions/skips;
- version checks;
- producer version checks;
- bounding checks;
- dirty propagations;
- dependency traversals;
- queue/ready-set operations;
- failed executions;
- State Store traffic;
- execution traces;
- parity and audit outcomes.

### Derived quantities

Parameterized cycle estimates:

C_model = sum_j n_j c_j.

### Hardware estimates

FPGA/RTL resource numbers remain feasibility estimates until synthesis, timing analysis, and hardware execution are performed.

## 16. Experimental matrix

The standard raster campaign uses requested mutation rates:

{0, 0.05, 0.10, 0.25, 0.50, 0.75, 1.0}

under clustered and dispersed locality.

The experiment reports:

p_o = mutated objects / objects
p_r = dirty TWRs / TWRs
p_e = executed TWRs / TWRs

The analysis must use p_e when discussing the recomputation fraction.

## 17. Required acceptance sequence

Results are interpreted in this order:

semantic lifecycle
-> dependency soundness
-> full-recompute oracle
-> Baseline C workset parity
-> output parity
-> measured operation accounting
-> parameterized timing
-> sensitivity analysis
-> hardware feasibility.

Skipping an earlier gate invalidates the interpretation of later performance comparisons.

## 18. Reproducibility

The experiment runner creates independent TWRF and B3 renderer instances, applies the same benchmark scene and mutation event, and compares frame-scoped execution sets and final outputs.

Machine-readable JSON records contain requested mutation, locality, p_o, p_r, p_e, parity flags, audit failures, and baseline/TWRF cycle totals.

## 19. Current non-goals

The current simulator does not establish:

- physical GPU performance;
- production-driver overhead;
- real vendor cache behavior;
- silicon energy efficiency;
- real SRAM timing;
- complete path-tracing scalability;
- synthesized FPGA utilization;
- FPGA timing closure.

These require separate physical validation.

## 20. Publication-safe interpretation

The project is strongest when described as an architectural study of a persistent spatial work-object abstraction, not as a claim that all incremental computation or temporal rendering is new.

The critical experiment is the comparison between TWRF and executable software incremental management under the same semantic workload.
