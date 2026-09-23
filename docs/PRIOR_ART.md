# TWRF Prior-Art and Novelty Boundary

**Purpose:** Define the evidence-supported research position of Temporal Work Region Fabric (TWRF) and prevent novelty claims that exceed the implementation and literature record.

## 1. The core distinction

TWRF is **not** proposed as a new form of incremental computation.

The literature already contains strong mechanisms for:

- dependency tracking
- dynamic dependence graphs
- memoization
- change propagation
- demand-driven incremental computation
- persistent named computation/cache locations
- incremental graphics and path-traced rendering.

The research contribution under evaluation is a particular **GPU architectural organization**:

[
oxed{
Persistent Spatial Work Identity
+
Persistent State
+
Version/Validity Tracking
+
Explicit Dependencies
+
Hardware-Oriented Scheduling
}
]

The relevant question is whether that combination, exposed as a first-class execution object in a GPU-like fabric, has useful architectural properties and under what workload/overhead conditions.

## 2. Self-adjusting computation

Self-adjusting computation is the strongest conceptual prior-art challenge.

Acar's work models computations that automatically adjust to external changes and develops dependence tracking and change propagation mechanisms. The associated experimental literature explicitly combines dynamic dependence graphs and memoization to identify and re-execute affected computation while reusing unaffected computation.

Implication for TWRF:

> Selective re-execution driven by dependencies is not novel by itself.

TWRF must therefore contribute at the architectural boundary: stable spatial work identity, persistent output/state, version validity, and hardware-oriented scheduling as a unified GPU execution abstraction.

See Acar's Carnegie Mellon doctoral thesis and the self-adjusting computation literature for the underlying model.

## 3. Named incremental computation and Adapton

Adapton extends incremental computation with demand-driven computation graphs and explicit names for reusable computation/cache locations.

This is important because persistent identity is also present in incremental-computation systems.

TWRF therefore does not claim that "persistent identity + dependency graph" is novel in the abstract. Its distinction is that identity is attached to a **spatial graphics execution object** and participates directly in a GPU-style scheduling and persistence model.

## 4. Persistent and temporal resources in graphics

Modern graphics render-graph systems already expose persistence.

AMD's Render Pipeline Shaders documentation distinguishes external, persistent, transient, and temporal resources; persistent resources can survive render-graph updates, and temporal resources permit access to historical slices.

PowerVR tile-based deferred rendering also establishes spatial tile decomposition and use of fast on-chip tile memory.

Implication:

> "Persistent graphics data" and "tile-local on-chip storage" are established ideas.

The TWRF claim is narrower: the persistent entity is not only a resource or attachment. It is a **persistent work object** carrying identity, dependency validity, output state, and execution lifecycle.

## 5. Incremental path-traced rendering

TU Wien work on incremental updates of path-traced scenes during editing explicitly partitions image-space regions and incrementally re-renders affected regions rather than immediately rebuilding the complete image. Follow-on work uses adaptive priority policies and automatically identifies and schedules affected regions.

This is close adjacent prior art to TWRF's regional temporal reuse.

Implication:

> Region-aware incremental rendering and affected-region scheduling already exist in graphics research.

TWRF differs in research scope by asking whether the persistent region itself should be represented as a hardware-level computational object with state, validity, dependency edges, and a common scheduler across raster, ray, and neural workloads.

## 6. What the simulator actually evaluates

The simulator is designed to separate the semantic question from the architectural-cost question.

### Baseline A
Unconditional full recomputation.

### Baseline B
A temporal-cache abstraction with tag lookup, validation, miss/refill and eviction costs.

### Baseline C
An executable software incremental scheduler using the same TWR graph, mutation events, kernels, persistent-state semantics, and semantic workset. Only the management mechanism differs: software B3 uses explicit scans and a software ready set; TWRF uses its architectural scheduler.

This makes the central comparison:

[
C_{TWRF} quad	ext{vs.}quad C_{B3}
]

rather than comparing TWRF only to an intentionally non-incremental baseline.

## 7. Adversarial equivalence test for competing mechanisms

A mechanism should be regarded as semantically TWR-equivalent when it can represent and maintain all of the following:

| Property | Required for TWR equivalence |
|---|---|
| Persistent identity | Yes |
| Spatial extent | Yes |
| Persistent output/state | Yes |
| Input version/validity | Yes |
| Dependency validity | Yes |
| Cross-frame selective execution | Yes |
| Explicit execution state | Yes |

This is intentionally a **semantic equivalence test**, not a claim that previous systems literally implement TWRF.

## 8. Defensible novelty statement

A research-paper-safe formulation is:

> Existing systems establish incremental computation, dependency-aware change propagation, persistent resources, spatial tiling, and incremental graphics independently or in partial combinations. TWRF investigates a GPU-oriented combination in which a persistent spatial computation is represented as a first-class work object carrying identity, state/output, input validity, dependency information, and scheduling state across frames. The work evaluates whether this organization reduces architectural management cost relative to full recomputation and an executable software incremental runtime.

The phrase "investigates" is deliberate. A publication claim of novelty should be made only after a formal literature review beyond this repository-level survey.

## 9. Key references

1. Umut A. Acar, *Self-Adjusting Computation*, Carnegie Mellon University PhD thesis, 2005.
2. Umut A. Acar et al., *A Library for Self-Adjusting Computation*, 2006.
3. Matthew A. Hammer et al., *Adapton: Composable, Demand-Driven Incremental Computation*, PLDI 2014.
4. Matthew A. Hammer et al., *Incremental Computation with Names*, OOPSLA 2015.
5. AMD GPUOpen, *RPS Tutorial Part 2 – Exploring Render Graphs and RPSL*, persistent/temporal resource documentation.
6. Imagination Technologies, *Tile-Based Deferred Rendering (TBDR)*, PowerVR architecture documentation.
7. Pascal Hann, *Incremental Updates of Path-Traced Scenes during Editing*, TU Wien, 2022.
8. Annalena Ulschmid, Bernhard Kerbl, Katharina Krösl, Michael Wimmer, *Real-Time Editing of Path-Traced Scenes with Prioritized Re-Rendering*, 2024.
9. Annalena Ulschmid et al., *Automated Prioritization for Context-Aware Re-rendering in Editing*, 2025.

## 10. Interpretation rule

The project must never use "novel", "first", "unprecedented", or similar absolute language for individual mechanisms such as temporal reuse, memoization, dependency tracking, persistent resources, or region-based incremental rendering.

The contribution under study is the **architectural combination and its measured/derived behavior**.
