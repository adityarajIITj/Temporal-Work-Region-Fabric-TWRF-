# TWRF-DOOM: Implementation Blueprint & Architecture Plan

**Project:** Integrating Classic DOOM with the Temporal Work Region Fabric (TWRF)  
**Target Branch:** `twrf-doom`  
**Companion Specifications:** [README.md](README.md), [docs/TECHNICAL_MANUAL.md](docs/TECHNICAL_MANUAL.md), [docs/TWRF_SPEC.md](docs/TWRF_SPEC.md)

---

## 1. Executive Summary & Research Motivation

### Why DOOM on TWRF?
In Phases 1 through 4, TWRF proved its core architectural hypothesis on synthetic scenes: persistent spatial work regions deliver up to **$1.83\times$ speedup** when scene volatility remains below the critical crossover point ($p^* \approx 13\%$), while incurring a predictable tracking tax when volatility is high.

To establish peer-reviewed scientific validity, TWRF must be tested on a **real, non-trivial, interactive gaming workload**. Classic DOOM (id Software, 1993) is the ideal benchmark:
1. **Heterogeneous Temporal Locality:**
   - **Status Bar (HUD):** Bottom 32 scanlines ($320 \times 32$) are static across consecutive frames except for occasional ammo/health counter updates (~$95\%$ temporal reuse).
   - **Skybox:** Vertically and horizontally invariant when walking forward/backward without turning (~$100\%$ temporal reuse during linear locomotion).
   - **Distant Static Architecture:** Distant walls and floors undergo minimal screen-space pixel shifts.
   - **Dynamic Entities:** Monsters, fireballs, and weapon animations occupy localized screen bounding boxes ($p \in [5\%, 25\%]$).
2. **Deterministic Replay:** DOOM's game loop (35 ticks/sec) and demo files (`.lmp`) provide 100% deterministic inputs, allowing bitwise repeatability and cycle-accurate tracking.
3. **Falsifiability:** DOOM will rigorously test whether TWRF's tracking overhead $C_t$ remains smaller than DOOM's software column/span rendering cost $C_r$.

---

## 2. DOOM Engine Core Selection: `doomgeneric`

We select **`doomgeneric`** (by ozkl / id Software) as the DOOM implementation:
- **Zero OS / Library Dependencies:** Unlike Chocolate Doom or Crispy Doom, `doomgeneric` does not require SDL2, X11, or DirectX.
- **Minimal Clean Hardware Abstraction:** The entire platform port interface consists of just 5 functions:
  ```c
  void DG_Init();
  void DG_DrawFrame();
  void DG_SleepMs(uint32_t ms);
  uint32_t DG_GetTicksMs();
  int DG_GetKey(int* pressed, unsigned char* key);
  ```
- **Framebuffer Driven:** Renders directly into a contiguous 32-bit linear framebuffer (`DG_ScreenBuffer`, $320 \times 200$ pixels).
- **WAD Compatibility:** Runs standard freeware/shareware `doom1.wad` (freely distributable, authentic id Software levels).

---

## 3. System Architecture & TWRF Integration

```
+===================================================================================+
|                              TWRF-DOOM ARCHITECTURE                               |
+===================================================================================+

  DOOM GAME ENGINE (doomgeneric C Core)
  +-------------------------------------------------------------------------------+
  | BSP Tree Traversal | Visplane Merging | Wall Columns | Sprite Sorter | HUD    |
  +---------------------------------------+---------------------------------------+
                                          | Framebuffer (320x200) + Game State
                                          v
  TWRF INTERCEPTION & CHANGE DETECTION LAYER
  +-------------------------------------------------------------------------------+
  | * Spatial Grid Partitioning: 320x200 -> 20x13 tiles (16x16 pixel TWR units)   |
  | * Game State Versioning: Player Pos (x, y, angle), Sector heights, HUD dirty  |
  | * Bounding Box Union: Monster & weapon sprite projected screen bounds         |
  +---------------------------------------+---------------------------------------+
                                          | Evaluates p = DirtyTiles / TotalTiles
                                          v
  TWRF PERSISTENT DATAFLOW SCHEDULER
  +-------------------------------------------------------------------------------+
  | Clean Tiles (Unchanged Sky / Walls / HUD)  -->  SKIPPED (0 Compute Cycles)    |
  | Dirty Tiles (Moving Monsters / Gun Fire)   -->  ENQUEUED to TEU ReadyQueue    |
  +---------------------------------------+---------------------------------------+
                                          | Dispatched
                                          v
  TILE EXECUTION UNITS (TEU) & STATE STORE
  +-------------------------------------------------------------------------------+
  | Recomputes only dirty tiles into persistent on-chip Logical State Store       |
  | Assembles final frame from Persistent Memory + newly rendered dirty tiles     |
  +-------------------------------------------------------------------------------+
```

### Tile Partitioning Configuration ($320 \times 200$)
- Frame Width: $320\text{ px}$
- Frame Height: $200\text{ px}$
- Tile Granularity: $16 \times 16\text{ px}$
- Grid Dimensions: $20\text{ columns} \times 13\text{ rows} = 260\text{ total TWR tiles}$ (bottom row clipped to 8 px).
- HUD Unit: Rows 11–13 (bottom 32 px) form a dedicated, temporally stable HUD TWR region.

---

## 4. Phased Implementation Roadmap

### Phase 1: Engine Embedding & Headless DOOM Harness
- [ ] Clone and integrate `doomgeneric` source files into `src/doom/` or dedicated subdirectory.
- [ ] Download and verify shareware `doom1.wad` asset.
- [ ] Implement `doomgeneric_twrf.cpp`:
  - Hook `DG_Init()` to initialize the TWRF virtual GPU simulator.
  - Hook `DG_DrawFrame()` to pass the DOOM frame buffer to TWRF.
  - Implement a headless timed benchmark runner capable of executing demo replays (e.g. `DEMO1`).
- [ ] Add CMake build target `twrf_doom`.

### Phase 2: Spatial Tile Partitioning & Game State Invalidation
- [ ] Wrap DOOM frame tiles into 260 `TemporalWorkRegion` instances within a `TWRGraph`.
- [ ] Create explicit `VersionedResource` handles for DOOM game state:
  - `PlayerTransformResource` (x, y, z, angle, pitch).
  - `SectorStateResource` (animated light sectors, crushing ceilings, opening doors).
  - `SpriteListResource` (monster coordinates, projectile trajectories).
  - `StatusBarResource` (health, armor, ammo, weapon flags).
- [ ] Implement conservative bounding box intersection:
  - When the player stands still and fires, only the weapon tile and bullet-hit tiles are dirtied.
  - When walking forward without turning, sky tiles remain clean.
  - When the player is idle, HUD and background tiles remain 100% clean.

### Phase 3: Persistent State Store & Incremental Composition
- [ ] Allocate 260 tile slots in TWRF's `LogicalStateStore` ($260 \times 16 \times 16 \times 4\text{ bytes} \approx 266\text{ KB}$ on-chip SRAM footprint).
- [ ] Implement incremental composition:
  - Clean tiles are bypassed; their existing colors are retained in persistent memory.
  - Dirty tiles are rendered and committed to the State Store.
- [ ] Enforce bitwise oracle parity: verify that TWRF incremental output matches vanilla unconditioned DOOM output bit-for-bit ($\Delta_{\text{oracle}} = 0$).

### Phase 4: Quantitative Evaluation & Break-Even Sweeps
- [ ] Measure effective skip ratio and cycle counts across 4 canonical DOOM gameplay scenarios:
  1. **Idle / Standing Still:** Player static, gun idle, minimal sector lighting ($p \approx 0\% \to 2\%$).
  2. **Linear Corridor Traversal:** Walking down E1M1 corridor without yaw rotation ($p \approx 10\% \to 20\%$).
  3. **High-Action Combat:** Imp fireballs, monster movement, shotgun animation ($p \approx 25\% \to 40\%$).
  4. **Rapid Yaw Turning / Room Transition:** 360-degree rapid turn dirtying all wall tiles ($p \approx 80\% \to 100\%$).
- [ ] Export machine-readable benchmark JSON (`results/doom_sweeps.json`).
- [ ] Generate comparative break-even plots (`results/doom_break_even.png`).

---

## 5. Branch & Commit Policy

In accordance with strict project rules:
- **Target Branch:** All commits, submodules, and code will be committed and pushed exclusively to **`twrf-doom`**.
- **Main Protection:** The `main` branch and `publication/final-freeze` will remain completely untouched.
- **Commit Messages:** Formatted cleanly under the conventional commit standard (`feat(doom): ...`, `docs(doom): ...`).
