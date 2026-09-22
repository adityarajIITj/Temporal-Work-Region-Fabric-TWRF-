# TWRF Architecture Specification v0.4

**Architecture Name:** Temporal Work Region Fabric (TWRF)  
**Status:** Unified System Specification (Sub-Plans 1–4 Complete)  
**Document Class:** Architecture Specification & Reference Contract

---

## 1. Core Hypothesis & Primitive

### 1.1 Research Hypothesis
Rendering and spatial computations benefit from persistent, spatially bounded execution state whose invocation is driven strictly by input and dependency version changes rather than unconditional, frame-wide recomputation.

### 1.2 The Temporal Work Region (TWR)
The fundamental computational unit of TWRF is the **Temporal Work Region (TWR)**. A TWR is a bounded persistent work unit that encapsulates:
1. **Explicit Identity (`TWRId`)**: A unique integer identifier across the execution graph.
2. **Region Descriptor (`BoundingRegion`)**: Geometric and spatial bounds (e.g., screen-space tile bounds, 3D scene bounding box).
3. **Execution Kernel (`KernelFunction`)**: A deterministic execution callback/operation that computes outputs from registered inputs.
4. **Input Bindings (`InputBinding`)**: A collection of references to `VersionedResource` instances along with the input version snapshot observed during the last execution.
5. **Output State**: A persistent allocation in the **Logical State Store**, tagged with an output version counter.
6. **Dependency Edges**:
   - **Upstream Producers (`dependencies`)**: TWRs whose outputs are direct inputs to this TWR.
   - **Downstream Consumers (`dependents`)**: TWRs that consume this TWR's output.
7. **Execution & Readiness State**:
   - `status`: `IdleClean`, `Dirty`, `Ready`, `Executing`.
   - `pending_dependencies`: Number of unfinished upstream producer TWRs.

---

## 2. Versioned Resource Model

### 2.1 Explicit Versioning
All data that enters or circulates within a TWRF system must be encapsulated in an explicit **`VersionedResource`**:
- Every mutation to a resource MUST monotonically increment its `VersionNumber` (`uint64_t`).
- **Forbidden**: Hidden global mutable memory, undeclared uniform mutations, or untracked side channels.

### 2.2 Reusability Rule
A TWR is reusable (eligible to skip execution) if and only if:
$$\forall i \in \text{Inputs}: \text{CurrentVersion}(i) = \text{RecordedVersion}(i)$$
AND no conservative spatial filter (e.g. bounding region mutation) has marked the region dirty.

---

## 3. Invalidation & Change Detection

### 3.1 Correctness Asymmetry
- **False Positives (Conservative Invalidation)**: Marking an unchanged TWR as dirty is mathematically safe and permissible. The TWR recomputes, yielding identical output at the cost of execution cycles.
- **False Negatives (Stale Reuse)**: Marking a changed TWR as clean produces incorrect, stale output. This constitutes a fatal architectural bug and must fail deterministic tests immediately.

### 3.2 Hashing Policy
Change detection relies solely on integer version vectors and conservative spatial bounding checks. Hash-based delta calculation is banned because the memory bandwidth required to hash intermediate buffers often exceeds the recomputation cost $C_r$ of shallow shaders.

---

## 4. Logical State Store

### 4.1 Persistence Model
Outputs of TWRs do not evaporate at frame boundaries. They persist in the **Logical State Store**:
- Outputs remain valid across arbitrary numbers of frames until explicitly invalidated or rewritten.
- Downstream consumers read directly from the State Store.

### 4.2 Accounting & Capacity Limits
The State Store models hardware storage constraints as configurable simulator parameters:
- Dual-port SRAM architecture with explicit read and write latencies.
- Tracks total allocated bytes, active slots, and peak high-water mark.
- Tracks read and write operations for memory movement cost estimation.
- Configurable capacity thresholds enable testing memory pressure without hardcoding physical hardware numbers.

---

## 5. Execution & Deterministic Scheduling

### 5.1 Eligibility vs. Readiness
- **Eligibility (Dirty State)**: A TWR needs to execute because an input resource or upstream producer changed.
- **Readiness (Dependency State)**: A TWR can only execute once all upstream producers have completed and committed their outputs (`pending_dependencies == 0`).
- An idle clean TWR is ready but not dirty $\implies$ skipped.
- A dirty TWR with pending dependencies is dirty but not ready $\implies$ waits in graph.
- A dirty TWR with zero pending dependencies $\implies$ placed in **Ready Queue**.

### 5.2 Dynamic Dependency Resolution (`prepare_frame`)
To prevent deadlock or false stalls when upstream producers are clean:
- Before frame execution, `prepare_frame` identifies the exact set of TWRs that will actually execute (`will_execute`).
- A dirty consumer initializes its `pending_dependencies` count to reflect **only** upstream producers that are in `will_execute`.
- If all upstream producers are clean, the dirty consumer enters the ready queue immediately.

### 5.3 Deterministic Tie-Breaking
The ready queue enforces deterministic ordering across repeated executions of identical workloads:
1. Priority level (higher priority executed first).
2. Dependency depth / topological rank (deeper nodes prioritized).
3. `TWRId` numerical ordering (lowest ID breaks any remaining tie).

---

## 6. Unified Execution Model for Heterogeneous Workloads

TWRF does not create separate, proprietary accelerator blocks (such as distinct "RT Cores" or "Tensor Cores") requiring CPU driver synchronizations. All three computational domains map directly to the unified TWR primitive:

### 6.1 3D Tile Rasterization TWRs
- **Granularity:** Screen-space tiles ($16 \times 16$ pixels).
- **Inputs:** Projection matrix, camera transform, mesh vertex buffers, material textures.
- **Output:** Tile color and depth buffers stored in the State Store.
- **Invalidation:** Geometry translations or camera movements trigger conservative 2D projected AABB overlap checks.

### 6.2 Bounded Ray Tracing Batches
- **Granularity:** Spatially coherent ray bundles (primary or secondary shadow rays).
- **Inputs:** Ray origin/direction descriptors, light position/direction, scene BVH.
- **Output:** Ray hit records (distance, normal, occlusion bitmask) committed to the State Store.
- **Invalidation:** Light translation or shadow occluder movement invalidates corresponding ray batches while leaving unchanged raster geometry clean.

### 6.3 Neural Reconstruction / Denoising MLPs
- **Granularity:** Feature blocks or image tiles.
- **Inputs:** G-buffer normals, albedo, noisy ray shadow mask, persistent recurrent feature tensors.
- **Output:** Reconstructed RGB color tensors.
- **Invalidation:** Triggered when upstream ray hit outputs update or when network weights change.
- **Temporal State:** Recurrent feature tensors are versioned resources within the State Store, ensuring explicit inter-frame state progression without hidden side-effects.

---

## 7. Architectural Timing & Baselines

### 7.1 Cycle Accounting Model
Every frame execution is accounted across five explicit cycle components:
$$C_{\text{total}} = C_{\text{change\_detect}} + C_{\text{schedule}} + C_{\text{execution}} + C_{\text{state\_store}} + C_{\text{interconnect}}$$

- $C_{\text{change\_detect}}$: $O(1)$ version checks for all bound inputs + bounding box checks for modified resources.
- $C_{\text{schedule}}$: Ready queue enqueue/dequeue operations based on priority and depth.
- $C_{\text{execution}}$: ALU, memory load, and texture sampling operations performed by active kernels.
- $C_{\text{state\_store}}$: Read/write bandwidth into dual-port SRAM banks.
- $C_{\text{interconnect}}$: Manhattan hop latency across the logical on-chip 2D mesh interconnect.

### 7.2 Independent Architectural Baselines
- **Baseline A (SIMT Full Recomputation):** Emulates a standard desktop GPU executing all regions unconditionally on every frame with zero tracking overhead ($C_t = 0$).
- **Baseline B (Conventional Temporal Cache):** Emulates a post-process software cache checking frame-level motion vectors, charging tag checks and cache lookups.

---

## 8. Architecture-Native API Boundary

The simulator exposes a clean, non-Vulkan command layer designed to reflect the hardware execution graph:

```
Application / Benchmark Scene
              ↓
Architecture-Native Command Layer (`twrf::api::TWRFEngine`)
              ↓
Heterogeneous Pipeline DAG Builder (`twrf::api::HeterogeneousPipeline`)
              ↓
TWRF Virtual GPU Simulator Core (`twrf::core::TWRScheduler`, `LogicalStateStore`)
              ↓
Execution Trace & Persistent Framebuffer Outputs
```

- Provides explicit stage binding (`add_stage`, `bind_input`).
- Supports step-by-step frame execution (`run_frame`).
- Enables forced full recomputation for oracle verification.

---

## 9. Architectural Deviations & Decisions Log

The following architectural deviations and corrections were established during implementation:

1. **Correction of the Break-Even Cost Formula (Sub-Plan 1 / Test S1-08):**
   - *Original exploratory note:* Stated $p > C_t / C_r$, which suggested temporal reuse is favored when volatility is high.
   - *Correction:* Formally corrected and proven to $p < 1 - C_t / C_r$. Temporal reuse is advantageous only when volatility $p$ is below $1 - C_t / C_r$.

2. **Bounding Box Evaluation Cost Accounting (Sub-Plan 3):**
   - *Original assumption:* Evaluated spatial bounding box overlap across all resources every frame.
   - *Correction:* In hardware, evaluating spatial bounding boxes for unmutated resources is redundant. The change tracker now performs $O(1)$ scalar version comparisons for all resources, and only executes bounding box overlap tests for resources whose version number actually changed.

3. **Dynamic Dependency Pruning in Scheduler (Sub-Plan 1 / Sub-Plan 4):**
   - *Original assumption:* A static dependency counter decremented only on kernel completion.
   - *Correction:* When upstream producers are clean (not dirty), they do not re-execute and therefore do not emit completion events. `TWRScheduler::prepare_frame` identifies the `will_execute` subset, allowing clean producers to be treated as satisfied immediately.

4. **Const Read Access with Mutable Bandwidth Tracking in State Store (Sub-Plan 1):**
   - *Original assumption:* Reading from `LogicalStateStore` was a mutating method, complicating const correctness across rendering queries.
   - *Correction:* `read_output` is const-qualified with `mutable StateStoreMetrics`, permitting clean read-only frame composition while maintaining byte-accurate SRAM traffic accounting.

5. **Unified Datapath vs. Dedicated Co-Processors (Sub-Plan 4):**
   - *Original consideration:* Creating separate specialized "TWR Tensor Core" and "TWR RT Core" blocks.
   - *Decision:* Replaced with a unified TWR execution contract. All three workloads execute as first-class TWR nodes scheduled by the same ready queue and reading/writing the same persistent State Store.
