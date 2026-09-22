# TWRF Prior-Art Map & Architectural Originality Taxonomy

This document provides a disciplined, non-hyperbolic taxonomy of the **Temporal Work Region Fabric (TWRF)** architecture, clearly delineating established prior art, architectural combinations, extensions, and unresolved research hypotheses.

---

## 1. Established Prior Art (Known Mechanisms)

The individual mechanical components utilized in TWRF build upon decades of graphics architecture, microarchitecture, and dataflow systems research:

| Mechanism | Representative Prior Art | How TWRF Uses It |
| :--- | :--- | :--- |
| **Tile-Based Deferred Rendering (TBDR)** | Imagination PowerVR, ARM Mali, Qualcomm Adreno, Apple Silicon | Partitioning the display target into bounded rectangular tiles ($8 \times 8$, $16 \times 16$, $32 \times 32$) to bound working set size. |
| **Temporal Reprojection & Frame Reuse** | Temporal Anti-Aliasing (TAA), NVIDIA DLSS, AMD FSR 2/3, Intel XeSS | Reusing computed color and depth information from preceding frames across motion vectors. |
| **Dataflow DAG Scheduling** | Static/dynamic dataflow (TRIPS, WaveScalar), coarse-grained reconfigurable arrays (CGRA) | Representing computation stages as a directed acyclic graph with explicit input edges and ready-queue dispatch. |
| **Hardware Ray Tracing Accelerators** | NVIDIA RT Cores (Turing/Ada), AMD Ray Accelerators (RDNA 2/3), Intel Xe RTU | Dedicated bounding volume hierarchy (BVH) and ray-primitive intersection hardware. |
| **Neural Rendering Accelerators** | NVIDIA Tensor Cores, Apple Neural Engine, Google TPU | Low-precision matrix-multiply-accumulate (MMA) engines executing small inference networks for denoising/upscaling. |

---

## 2. Architectural Combinations Implemented in TWRF

TWRF does not claim that tiles, rays, MLPs, or caches are unprecedented. Rather, TWRF explores the architectural synthesis of these concepts into a unified execution model:

1. **Cross-Workload Common Execution Contract**:
   - Rather than deploying physically segregated, heterogeneous fixed-function blocks (e.g., separate "Tensor Cores" vs. "RT Cores" vs. "Shader Cores") with proprietary driver-managed handoffs, TWRF executes raster tiles, ray batches, and neural blocks as instances of the **same fundamental primitive**: the **Temporal Work Region (TWR)**.
2. **Persistent Inter-Frame State Store**:
   - Conventional GPUs treat on-chip tile buffers (Tile Memory) as transient scratchpads that are cleared or written out to VRAM at the conclusion of each render pass. TWRF models a persistent on-chip **Logical State Store** where outputs remain resident across frame boundaries, keyed by region identity and version tags.
3. **Version-Driven Hardware Scheduling**:
   - Rather than relying on software shaders or compute dispatches to inspect motion vectors and reproject samples, change detection is integrated into the scheduling frontier: scalar version checks and conservative spatial bounds dictate execution eligibility before work reaches compute ALUs.

---

## 3. Extensions & Unresolved Novelty

The primary research hypotheses being tested by TWRF that require ongoing investigation are:

### Hypothesis 1: Heterogeneous Cross-Stage Invalidation Without CPU Intervention
- In traditional graphics APIs (Vulkan/DirectX 12), orchestrating a pipeline where a raster G-buffer feeds ray-traced shadows which feed a neural denoiser requires explicit render passes, command buffers, and pipeline barriers.
- TWRF models whether hardware-level producer-consumer dependency edges can propagate dirty states automatically across heterogeneous passes. For instance, when a light source moves, TWRF demonstrates that the raster stage is automatically skipped while ray and neural stages execute, with zero CPU driver overhead.

### Hypothesis 2: Hardware-Governed Dynamic Break-Even Fallback
- Conventional temporal reuse algorithms (TAA/FSR) often suffer from ghosting or performance regressions during violent scene changes because the software algorithm must detect failure retrospectively.
- TWRF proposes an integrated hardware mechanism where the ratio $C_t / C_r$ and change fraction $p$ are dynamically tracked to transition gracefully between persistent dataflow execution and unconditioned full recomputation.

---

## 4. Prior-Art Comparison Matrix

| Feature | Modern Desktop SIMT (NVIDIA / AMD) | Mobile TBDR (Apple / Qualcomm) | Temporal Super-Resolution (DLSS / FSR) | Proposed TWRF Architecture |
| :--- | :--- | :--- | :--- | :--- |
| **Primary Execution Model** | SIMT warp/wavefront dispatch | Tile binning + fragment dispatch | Post-process compute shaders | Persistent dataflow DAG |
| **Inter-Frame Intermediate Reuse** | Uncached across frame boundaries | Flushed to DRAM at frame end | Software history buffers in VRAM | Persistent on-chip Logical State Store |
| **Change Detection Mechanism** | None (unconditional recompute) | None (unconditional recompute) | Motion vector reprojection in shader | Hardware version tags + bounding checks |
| **Ray Tracing Granularity** | Individual ray queries / hits | Software hybrid or dedicated RTU | N/A | Bounded ray batches in TWR nodes |
| **Neural Integration** | Tensor MMA instructions in SM | Co-processor / NPU dispatch | Execution in compute pipeline | TWR graph node with temporal state |
| **Worst-Case Cost Penalty** | 0% (baseline is full recompute) | 0% (baseline is full recompute) | Compute shader overhead on fast moves | $+176.4\%$ tracking tax at $p=1.0$ (documented) |

---

## 5. Summary & Novelty Posture

TWRF is a scientific research architecture designed to falsify or validate whether persistent spatial work regions can deliver net energy/cycle advantages over unconditional recomputation in real-time graphics. Claims of "first ever" or "universal GPU replacement" are unsupported and explicitly disclaimed. The value of TWRF lies in its rigorous, evidence-based quantification of where temporal dataflow succeeds ($p \le 10\%$) and where it fails ($p > 13\%$).
