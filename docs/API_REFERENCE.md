# TWRF C++ API Reference Manual

**Namespace:** `twrf`  
**Language Standard:** C++20  
**Scope:** Core Virtual GPU Simulator, 3D Tiled Rasterizer, Microarchitectural Timing Model, Ray Tracing, Neural MLPs, and Heterogeneous Pipeline API.

---

## Table of Contents

1. [Core Virtual GPU Primitives (`twrf::core`)](#1-core-virtual-gpu-primitives-twrfcore)
   - [Types & Enums](#types--enums)
   - [VersionedResource](#versionedresource)
   - [TemporalWorkRegion (TWR)](#temporalworkregion-twr)
   - [TWRGraph](#twrgraph)
   - [LogicalStateStore](#logicalstatestore)
   - [TWRScheduler](#twrscheduler)
   - [ChangeTracker](#changetracker)
   - [CostModel](#costmodel)
2. [3D Tiled Rasterization (`twrf::raster`)](#2-3d-tiled-rasterization-twrfraster)
   - [Linear Algebra & Math](#linear-algebra--math)
   - [Geometry & Textures](#geometry--textures)
   - [TileRasterizer & Framebuffer](#tilerasterizer--framebuffer)
   - [TWRFRenderer](#twrfrenderer)
   - [WorkloadGenerator](#workloadgenerator)
3. [Timing & Baselines (`twrf::timing`)](#3-timing--baselines-twrftiming)
   - [TimingParams](#timingparams)
   - [CycleAccounting](#cycleaccounting)
   - [Hardware Baselines (A & B)](#hardware-baselines-a--b)
   - [ExperimentRunner](#experimentrunner)
4. [Bounded Ray Tracing (`twrf::ray`)](#4-bounded-ray-tracing-twrfray)
   - [Ray Types & Batches](#ray-types--batches)
   - [BoundedRayTracer](#boundedraytracer)
5. [Neural Reconstruction (`twrf::neural`)](#5-neural-reconstruction-twrfneural)
   - [FeatureTensor](#featuretensor)
   - [TinyMLP](#tinymlp)
6. [Architecture-Native Command API (`twrf::api`)](#6-architecture-native-command-api-twrfapi)
   - [HeterogeneousPipeline](#heterogeneouspipeline)
   - [TWRFEngine](#twrfengine)

---

## 1. Core Virtual GPU Primitives (`twrf::core`)

Header: `#include "twrf/core/twrf_core.hpp"` (or individual headers under `twrf/core/`).

### Types & Enums
Defined in [`include/twrf/core/types.hpp`](../include/twrf/core/types.hpp).

```cpp
using TWRId = uint32_t;
using ResourceId = uint32_t;
using VersionNumber = uint64_t;

enum class TWRStatus {
    IdleClean, // Up to date with all inputs; eligible to skip execution
    Dirty,     // At least one bound input or upstream producer has changed
    Ready,     // Dirty and all upstream dependencies are satisfied
    Executing  // Currently executing on a Tile Execution Unit (TEU)
};

struct BoundingRegion {
    float min_x, min_y, max_x, max_y;
    bool intersects(const BoundingRegion& other) const noexcept;
    static BoundingRegion full_screen();
};
```

---

### VersionedResource
Defined in [`include/twrf/core/resource.hpp`](../include/twrf/core/resource.hpp).

Represents any piece of state (matrix, vertex buffer, texture, light, tensor) whose changes trigger invalidation.

```cpp
class VersionedResource {
public:
    VersionedResource(ResourceId id, std::string name, BoundingRegion bounds = BoundingRegion::full_screen());

    ResourceId id() const noexcept;
    const std::string& name() const noexcept;
    VersionNumber version() const noexcept;
    const BoundingRegion& bounds() const noexcept;

    // Mutates resource: increments version monotonically and updates spatial bounds
    void bump_version(const BoundingRegion& new_bounds);
    void bump_version();

    // Attach arbitrary payload
    template<typename T> void set_data(T&& data);
    template<typename T> const T& get_data() const;
};
```

---

### TemporalWorkRegion (TWR)
Defined in [`include/twrf/core/twr.hpp`](../include/twrf/core/twr.hpp).

The fundamental computational primitive.

```cpp
using KernelFunction = std::function<void(const std::vector<const VersionedResource*>& inputs,
                                          std::vector<uint8_t>& output_state)>;

class TemporalWorkRegion {
public:
    TemporalWorkRegion(TWRId id, std::string name, BoundingRegion bounds, int32_t priority = 0);

    TWRId id() const noexcept;
    TWRStatus status() const noexcept;
    int32_t priority() const noexcept;
    uint32_t topological_depth() const noexcept;
    VersionNumber output_version() const noexcept;

    void bind_input(const VersionedResource* resource);
    void add_dependency(TWRId upstream_id);
    void set_kernel(KernelFunction kernel);

    bool check_clean() const; // O(1) comparison against recorded input versions
    void execute(std::vector<uint8_t>& output_state);
};
```

---

### LogicalStateStore
Defined in [`include/twrf/core/state_store.hpp`](../include/twrf/core/state_store.hpp).

Models on-chip persistent dual-port SRAM with byte-accurate accounting.

```cpp
class LogicalStateStore {
public:
    explicit LogicalStateStore(StateStoreConfig config = {});

    void allocate(TWRId id, size_t size_bytes);
    void write_output(TWRId id, const void* data, size_t size_bytes, VersionNumber version);
    const std::vector<uint8_t>& read_output(TWRId id) const; // Const-qualified; increments read_bytes

    size_t allocated_bytes() const noexcept;
    size_t capacity_bytes() const noexcept;
    StateStoreMetrics metrics() const noexcept;
    void reset_metrics();
};
```

---

### TWRScheduler
Defined in [`include/twrf/core/scheduler.hpp`](../include/twrf/core/scheduler.hpp).

Drives deterministic dataflow execution with 3-tier tie-breaking.

```cpp
class TWRScheduler {
public:
    TWRScheduler(TWRGraph& graph, LogicalStateStore& state_store);

    void prepare_frame(); // Marks dirty TWRs and seeds ready queue with active dependencies
    uint64_t step_frame(); // Executes all ready TWRs until queue empty; returns executed count
    void force_full_execution(); // Fallback oracle mode: marks 100% of TWRs dirty

    const ExecutionTrace& trace() const noexcept;
    SchedulerMetrics metrics() const noexcept;
};
```

---

### CostModel
Defined in [`include/twrf/core/cost_model.hpp`](../include/twrf/core/cost_model.hpp).

Encodes the corrected break-even formula:

```cpp
class CostModel {
public:
    static double expected_baseline_cost(double C_r); // Returns C_r
    static double expected_twrf_cost(double C_t, double C_r, double p); // C_t + p * C_r
    static double break_even_threshold(double C_t, double C_r); // 1.0 - (C_t / C_r)
    static bool does_twrf_win(double C_t, double C_r, double p); // p < 1.0 - (C_t / C_r)
};
```

---

## 2. 3D Tiled Rasterization (`twrf::raster`)

Header: `#include "twrf/raster/renderer.hpp"`.

### TileConfig & TWRFRenderer
Controls screen resolution, tile subdivision, and frame rendering.

```cpp
struct TileConfig {
    uint32_t frame_width = 128;
    uint32_t frame_height = 128;
    uint32_t tile_size = 16; // 8x8 = 64 tiles
    uint32_t tiles_x() const noexcept;
    uint32_t tiles_y() const noexcept;
    uint32_t total_tiles() const noexcept;
};

class TWRFRenderer {
public:
    explicit TWRFRenderer(TileConfig config);

    void initialize();
    Scene& scene() noexcept;
    TileRasterizer& rasterizer() noexcept;

    // Normal persistent execution: executes only dirty tiles
    FrameMetrics render_frame_incremental();

    // Oracle reference execution: unconditionally executes all tiles
    FrameMetrics render_frame_oracle();

    const Framebuffer& get_framebuffer() const noexcept;
};
```

### FrameMetrics
```cpp
struct FrameMetrics {
    uint32_t tiles_executed = 0;
    uint32_t tiles_skipped = 0;
    double skip_ratio = 0.0;
    Framebuffer framebuffer;
};
```

---

## 3. Timing & Baselines (`twrf::timing`)

Header: `#include "twrf/timing/experiment_runner.hpp"`.

### TimingParams & CycleAccounting
Configurable hardware microarchitectural latencies:

```cpp
struct TimingParams {
    double change_check_cycles_per_resource = 5.0;
    double bbox_intersection_cycles = 10.0;
    double schedule_push_cycles = 15.0;
    double schedule_pop_cycles = 15.0;
    double state_store_read_cycles_per_byte = 0.05;
    double state_store_write_cycles_per_byte = 0.08;
    double interconnect_cycles_per_hop = 3.0;
};

struct CycleAccounting {
    double change_detection_cycles = 0.0;
    double scheduler_cycles = 0.0;
    double execution_cycles = 0.0;
    double state_store_cycles = 0.0;
    double interconnect_cycles = 0.0;

    double total_cycles() const noexcept;
    double tracking_overhead_cycles() const noexcept;
};
```

### ExperimentRunner
Executes systematic parameter sweeps and exports machine-readable JSON:

```cpp
class ExperimentRunner {
public:
    static std::vector<SweepDataPoint> run_change_rate_sweep(uint32_t res = 128, uint32_t tile_sz = 16);
    static std::vector<SweepDataPoint> run_locality_sweep(uint32_t res = 128, uint32_t tile_sz = 16);
    static void export_json(const std::string& path,
                           const std::vector<SweepDataPoint>& change_sweep,
                           const std::vector<SweepDataPoint>& locality_sweep);
};
```

---

## 4. Bounded Ray Tracing (`twrf::ray`)

Header: `#include "twrf/ray/ray_tracer.hpp"`.

### RayBatch & BoundedRayTracer
Encapsulates bounded bundles of primary and shadow rays for TWR execution:

```cpp
struct RayBatch {
    uint32_t batch_id;
    std::vector<Ray> rays;
    RayAABB bounding_box;
};

class BoundedRayTracer {
public:
    explicit BoundedRayTracer(RayScene scene);

    // Executes ray batch intersection kernel
    std::vector<HitRecord> trace_batch(const RayBatch& batch) const;

    // Fast shadow test against scene lights
    std::vector<float> trace_shadow_batch(const RayBatch& batch, const raster::Vec3& light_pos) const;
};
```

---

## 5. Neural Reconstruction (`twrf::neural`)

Header: `#include "twrf/neural/mlp.hpp"`.

### FeatureTensor & TinyMLP
Deterministic inference engine executing multi-layer perceptrons for temporal denoising:

```cpp
class FeatureTensor {
public:
    FeatureTensor(std::vector<size_t> dims, float init_val = 0.0f);
    float& at(size_t i, size_t j);
    float at(size_t i, size_t j) const;
    const std::vector<float>& data() const noexcept;
};

class TinyMLP {
public:
    TinyMLP(size_t in_dim, const std::vector<size_t>& hidden_dims, size_t out_dim);

    FeatureTensor forward(const FeatureTensor& input) const;
    FeatureTensor forward_temporal(const FeatureTensor& input, FeatureTensor& recurrent_state) const;

    void set_weights(size_t layer_idx, const FeatureTensor& w, const std::vector<float>& b);
};
```

---

## 6. Architecture-Native Command API (`twrf::api`)

Header: `#include "twrf/api/heterogeneous_pipeline.hpp"`.

### HeterogeneousPipeline
Orchestrates cross-domain DAGs connecting Raster $\to$ Ray $\to$ Neural stages:

```cpp
class HeterogeneousPipeline {
public:
    explicit HeterogeneousPipeline(uint32_t num_tiles = 4);

    void initialize();
    uint64_t run_frame(); // Returns number of executed TWRs across all stages

    void notify_light_moved(const raster::Vec3& new_pos);
    void notify_geometry_moved(uint32_t tile_idx);
    void force_full_recomputation();

    const std::vector<uint8_t>& get_tile_output(uint32_t tile_idx) const;
};
```

#### Example Usage:
```cpp
twrf::api::HeterogeneousPipeline pipe(4);
pipe.initialize();

// Cold start (all 12 nodes execute)
pipe.run_frame();

// Move light (only ray + neural re-execute; raster skipped)
pipe.notify_light_moved(twrf::raster::Vec3(10.0f, 15.0f, 5.0f));
uint64_t exec_count = pipe.run_frame();
assert(exec_count == 8); // 4 Ray + 4 Neural
```
