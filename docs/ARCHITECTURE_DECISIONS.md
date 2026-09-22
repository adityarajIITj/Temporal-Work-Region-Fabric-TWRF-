# Architecture Decision Records (ADR) — TWRF Project

**Status:** Active & Complete  
**Milestone:** Sub-Plans 1 through 4

---

## ADR-001: Integer Version Vectors vs. Hash-Based Delta Signatures
* **Context**: Earlier exploratory reports suggested generating 64-bit to 256-bit cryptographic or non-cryptographic hashes of tile inputs to detect changes.
* **Decision**: Change detection is driven strictly by monotonic integer version numbers attached to explicit `VersionedResource` objects and conservative bounding region intersections. Hashing is banned.
* **Rationale**: Hashing every tile or input buffer incurs a massive memory read tax (e.g., up to 129 MB per frame in a 1080p rasterizer), frequently exceeding the recomputation cost $C_r$ of simple shaders. Integer counters cost $O(1)$ scalar comparisons and are mathematically sound.

---

## ADR-002: Configurable Logical State Store vs. Hardcoded Hardware Claims
* **Context**: Research papers often claim fixed SRAM sizes (e.g., 64–128 MB on-chip SRAM).
* **Decision**: The virtual GPU State Store is modeled logically. Capacities, allocation sizes, read/write latencies, and spill limits are simulator parameters provided via configuration, not immutable constants.
* **Rationale**: Hardware specifications must remain falsifiable experimental variables in research simulation.

---

## ADR-003: Strict Deterministic Ready-Queue Tie-Breaking
* **Context**: Workloads with independent branches can execute in arbitrary order if tie-breaking is not explicitly defined, leading to non-deterministic execution traces.
* **Decision**: The ready queue enforces a strict three-tier comparison:
  1. Priority (`int32_t`, higher first).
  2. Topological depth (`uint32_t`, deeper nodes first).
  3. Unique identifier (`TWRId`, lower ID first).
* **Rationale**: Reproducibility is mandatory across all sub-plans. Two identical simulation runs must produce identical bitwise event traces.

---

## ADR-004: Dynamic Dependency Pruning via Active Set Pre-Computation
* **Context**: In persistent dataflow graphs, upstream producers that are clean do not execute during the current frame and therefore do not emit completion events. If downstream consumers wait for all declared producers unconditionally, they dead-lock or fail to execute when their other inputs change.
* **Decision**: The scheduler executes `prepare_frame()` prior to dispatch, computing the exact subset of nodes that will execute (`will_execute`). Only upstream producers in `will_execute` are counted in a consumer's active `pending_dependencies`.
* **Rationale**: Clean producers already have valid, committed state in the persistent State Store. Waiting for clean nodes to re-execute violates dataflow semantics.

---

## ADR-005: Unified TWR Execution Contract for Heterogeneous Workloads
* **Context**: Traditional GPU architectures introduce disjoint, proprietary fixed-function co-processors (e.g., dedicated RT Cores and Tensor Cores) managed by explicit CPU driver synchronizations and pipeline barriers.
* **Decision**: Raster tiles, ray tracing batches, and neural MLP denoising blocks are implemented as instances of the unified `TemporalWorkRegion` primitive, using the same scheduler, ready queue, and persistent State Store.
* **Rationale**: Isolates whether persistent dataflow and temporal reuse provide architectural benefits across heterogeneous workloads without convolving results with proprietary fixed-function ASIC logic.

---

## ADR-006: Dual-Port State Store with Const-Qualified Access and Internal Accounting
* **Context**: The `LogicalStateStore` must accurately count read/write memory traffic while allowing the 3D renderer and downstream queries to read cached framebuffers using clean `const` semantics.
* **Decision**: `read_output()` is const-qualified with internal counters encapsulated in `mutable StateStoreMetrics`.
* **Rationale**: Prevents artificial `const_cast` workarounds and ensures thread-safe read-only inspection without compromising microarchitectural accounting fidelity.

---

## ADR-007: Analytical Bounding Box Evaluation Restricted to Mutated Resources
* **Context**: Naively performing 2D bounding box intersection tests against all scene resources for every screen tile incurs an $O(N_{\text{tiles}} \times N_{\text{resources}})$ computational tax on every frame, even when no resources mutated.
* **Decision**: The change tracker performs $O(1)$ scalar version comparisons for all registered resources, and executes spatial bounding box intersection tests **only** for resources whose version counter has actually incremented.
* **Rationale**: Matches physical hardware scoreboard behavior and prevents tracking overhead $C_t$ from becoming artificially dominated by idle scene elements.
