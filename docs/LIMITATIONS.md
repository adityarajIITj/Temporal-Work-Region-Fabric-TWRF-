# TWRF Architectural Limitations & Known Failure Regimes

This document transparently records the architectural boundaries, dominant overheads, and losing regimes of the **Temporal Work Region Fabric (TWRF)** virtual GPU architecture, grounded in experimental measurements from Sub-Plan 3 and Sub-Plan 4.

---

## 1. The Break-Even Boundary: $p^* \approx 13\%$

In accordance with the corrected break-even equation:
$$p < 1 - \frac{C_t}{C_r}$$

Temporal reuse is beneficial **only** when scene volatility $p$ remains below the critical threshold $p^*$. 

Under default architectural parameters (64 tiles of $16 \times 16$ pixels on a $128 \times 128$ frame buffer):
- **Winning Regime ($p \le 10\%$)**: TWRF delivers up to **1.83x cycle reduction** (8,716.8 vs. 15,984.0 cycles) on static and slow-moving scenes.
- **Break-Even Crossover ($p^* \approx 13\%$)**: Beyond 13% changed regions, tracking overhead $C_t$ negates any computation savings from temporal reuse.
- **Losing Regime ($p \ge 25\%$)**: TWRF consumes strictly more cycles than conventional full recomputation:
  - At $p = 25\%$: $+27.4\%$ cycle penalty.
  - At $p = 50\%$: $+43.4\%$ cycle penalty.
  - At $p = 75\%$: $+96.2\%$ cycle penalty.
  - At $p = 100\%$ (Worst-case): **$+176.4\%$ cycle penalty** (44,182.4 vs. 15,984.0 cycles).

---

## 2. Dominant Overhead Components ($C_t$)

Experimental profiling reveals three primary contributors to tracking overhead:

| Overhead Component | Measured Cycles (p=0.0) | Measured Cycles (p=1.0) | Mechanism |
| :--- | :---: | :---: | :--- |
| **Change Detection ($C_{\text{change\_detect}}$)** | 5,440.0 | 15,680.0 | Version comparison fan-out ($N_{\text{tiles}} \times N_{\text{inputs}}$) plus bounding box overlap tests for mutated resources. |
| **State Store Read ($C_{\text{state\_store}}$)** | 3,276.8 | 3,276.8 | On-chip SRAM bandwidth required to read persistent tile states for final frame composition. |
| **Scheduler Queue ($C_{\text{schedule}}$)** | 0.0 | 1,920.0 | Priority queue push, topological depth tie-breaking, and pop operations. |
| **Interconnect Hops ($C_{\text{interconnect}}$)** | 0.0 | 576.0 | Logical 2D NoC routing between tile execution units and central state memory. |

### Key Architectural Bottleneck: Input Version Fan-Out
Each tile TWR tracks dependencies on scene camera, textures, and geometry resources. Even with clean tiles, evaluating version numbers across 64 tiles $\times$ 17 resources incurs $1,088$ comparisons. As scene complexity scales to thousands of objects, naive per-tile version iteration becomes prohibitive without hierarchical spatial BVH culling.

---

## 3. Pathological Workload Regimes

TWRF is provably suboptimal under the following workload characteristics:

### 1. Rapid Global Camera Movement / Fast Pan
- When the camera translates or rotates significantly between frames, virtually all screen tiles undergo projected coordinate shifts.
- Every tile's input version changes, triggering conservative invalidation across the entire frame.
- **Result**: 100% of tiles re-execute ($p = 1.0$), paying full recompute work $C_r$ **plus** the complete tracking overhead $C_t$.

### 2. Spatially Dispersed High-Frequency Dynamics
- Workloads such as rain particles, blowing snow, dense foliage flutter, or full-screen noisy post-processing.
- Although the volume of changed geometry may be small, it touches almost every screen tile.
- While clustered motion ($p=0.25$) confines dirty tiles to a localized subset, dispersed motion with the identical change fraction dirties up to 2x more tiles, drastically shifting the break-even threshold downwards.

### 3. Divergent Secondary Ray Distributions
- Primary rays and shadow rays exhibit high spatial coherence and benefit cleanly from bounded batching.
- In contrast, diffuse interreflection or glossy indirect rays scatter arbitrarily across the scene.
- A single distant moving object can invalidate a large fraction of ray batches, leading to high invalidation fan-out and low reuse.

### 4. Low Arithmetic Intensity (Shallow Shaders)
- When region recompute cost $C_r$ is very small (e.g. flat unshaded quads with 2 triangles), $C_t / C_r$ is large, driving $1 - C_t / C_r$ close to zero or negative.
- TWRF requires computationally dense workloads (heavy procedural shading, complex geometry, multi-bounce shadow testing, neural reconstruction) for temporal reuse to justify its tracking cost.

---

## 4. Architectural Mitigation & Fallback Policy

To ensure robustness, the TWRF architecture mandates:
1. **Dynamic Fallback Mode**: When the measured change rate exceeds $p^* \approx 13\%$ over a sliding window of frames, the hardware scheduler disables tracking checks and transitions to unconditioned full recompute mode.
2. **Hierarchical Spatial Bounding**: Resource bindings must be pruned against a coarse hierarchical bounding volume hierarchy (BVH) before tile-level version checks to prevent $O(N_{\text{tiles}} \times N_{\text{objects}})$ tracking overhead.
3. **Bounded State Store Spilling**: If active state exceeds on-chip SRAM capacity, low-priority clean tiles are evicted to DRAM, incurring documented DRAM transfer latency rather than causing buffer overflow.
