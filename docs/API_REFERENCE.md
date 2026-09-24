# TWRF C++ API Reference

Status: Current research implementation on branch research/p0-semantic-hardening.
Language: C++20.

This document describes the implemented API. Planned or hardware-only interfaces are not represented as implemented functions.

## 1. Core types

Header: include/twrf/core/types.hpp

using TWRId = uint32_t;
using ResourceId = uint32_t;
using VersionNumber = uint64_t;
using Timestamp = uint64_t;

TWRStatus:
- IdleClean
- Dirty
- Ready
- Executing
- Failed

ExecutionReason:
- None
- Initial
- InputVersionChanged
- ProducerOutputChanged
- ConservativeInvalidation
- ForcedRecompute

BoundingRegion contains integer min/max coordinates and provides is_empty(), overlaps(), create(), and full_screen().

## 2. VersionedResource

Header: include/twrf/core/resource.hpp

VersionedResource represents explicitly tracked mutable input state.

Implemented interface includes:

ResourceId id() const noexcept;
const std::string& name() const noexcept;
VersionNumber version() const noexcept;
const BoundingRegion& bounds() const noexcept;
void set_value(...);
void set_data(...);
void set_bounds(...);
void bump_version();

The exact set_value/set_data overloads are template-based and depend on the payload type.

## 3. TemporalWorkRegion

Header: include/twrf/core/twr.hpp

Kernel contract:

using KernelCallback = std::function<bool(
    TemporalWorkRegion& self,
    LogicalStateStore& state_store,
    const std::vector<const VersionedResource*>& inputs)>;

Construction:

TemporalWorkRegion(
    TWRId id,
    std::string name,
    BoundingRegion region = BoundingRegion::full_screen(),
    int32_t priority = 0);

Inspection:

TWRId id() const noexcept;
const std::string& name() const noexcept;
const BoundingRegion& region() const noexcept;
TWRStatus status() const noexcept;
ExecutionReason execution_reason() const noexcept;
int32_t priority() const noexcept;
uint32_t topological_depth() const noexcept;
uint32_t pending_dependencies() const noexcept;
VersionNumber current_output_version() const noexcept;
uint64_t total_executions() const noexcept;
uint64_t total_skips() const noexcept;

Lifecycle and graph metadata:

void set_region(const BoundingRegion& region) noexcept;
void set_priority(int32_t priority) noexcept;
void set_topological_depth(uint32_t depth) noexcept;
void set_kernel(KernelCallback kernel);
void bind_resource(ResourceId id, VersionNumber recorded = INVALID_VERSION);
void add_upstream_dependency(TWRId producer_id,
                             VersionNumber recorded = INVALID_VERSION);
void add_downstream_consumer(TWRId consumer_id);
void mark_dirty(ExecutionReason reason) noexcept;
void set_ready() noexcept;
void set_executing() noexcept;
void mark_clean() noexcept;
void mark_failed(ExecutionReason reason = ExecutionReason::None) noexcept;
void reset_pending_dependencies() noexcept;
void set_pending_dependencies(uint32_t count) noexcept;
void decrement_pending_dependencies() noexcept;

Execution:

bool execute(
    LogicalStateStore& state_store,
    const std::vector<const VersionedResource*>& inputs,
    Timestamp step);

## 4. Dependency audit

The runtime dependency audit is opt-in.

void enable_dependency_audit(bool enabled = true) noexcept;
bool dependency_audit_enabled() const noexcept;

void observe_resource(ResourceId id);
void observe_upstream_producer(TWRId producer_id);

const std::unordered_set<ResourceId>& observed_resource_reads() const noexcept;
bool dependency_audit_was_evaluated() const noexcept;
bool dependency_audit_passed_last_execution() const noexcept;
bool dependency_audit_passes() const noexcept;
std::string dependency_audit_report() const;

An audited execution fails when it observes a mutable resource or upstream producer that is not declared by the TWR.

The audit is a runtime contract checker. It does not automatically instrument arbitrary C++ memory loads.

## 5. TWRGraph

Header: include/twrf/core/graph.hpp

VersionedResource& add_resource(
    ResourceId id,
    std::string name,
    BoundingRegion bounds = BoundingRegion::full_screen());

TemporalWorkRegion& add_twr(
    TWRId id,
    std::string name,
    BoundingRegion region = BoundingRegion::full_screen(),
    int32_t priority = 0);

void connect_dependency(TWRId producer_id, TWRId consumer_id);
void bind_resource(TWRId twr_id, ResourceId resource_id);

VersionedResource* get_resource(ResourceId id) const noexcept;
TemporalWorkRegion* get_twr(TWRId id) const noexcept;

const auto& resources() const noexcept;
const auto& twrs() const noexcept;

bool validate_and_compute_depths();

connect_dependency creates both producer-side downstream and consumer-side upstream graph metadata.

## 6. LogicalStateStore

Header: include/twrf/core/state_store.hpp

explicit LogicalStateStore(size_t capacity_limit_bytes = 0);

void set_capacity_limit(size_t limit_bytes) noexcept;
const StateStoreMetrics& metrics() const noexcept;
void reset_metrics() noexcept;

bool has_slot(TWRId id) const noexcept;
void allocate_slot(TWRId id, size_t initial_capacity = 0);

bool write_output(
    TWRId id,
    const void* src,
    size_t size_bytes,
    VersionNumber version,
    Timestamp step = 0);

const uint8_t* read_output(
    TWRId id,
    size_t& out_size,
    VersionNumber& out_version) const;

VersionNumber get_output_version(TWRId id) const noexcept;
size_t slot_count() const noexcept;

StateStoreMetrics records current/peak allocation, read/write operation counts, byte traffic, and capacity.

A rejected capacity-limited write does not create a slot or update the producer's output version.

## 7. Metrics

Header: include/twrf/core/metrics.hpp

MetricsCollector currently records:

frames_executed
total_twr_executions
total_twr_skips
dependency_traversals
dirty_evaluations
dependency_audit_passes
dependency_audit_failures
resource_version_checks
producer_version_checks
bounding_checks
ready_queue_pushes
ready_queue_pops
dirty_propagations
failed_executions

skip_ratio() returns the skip fraction over executions plus skips.

## 8. TWRScheduler

Header: include/twrf/core/scheduler.hpp

size_t ready_queue_size() const noexcept;
Timestamp current_step() const noexcept;

void prepare_frame(
    TWRGraph& graph,
    LogicalStateStore& state_store,
    ExecutionTrace& trace,
    MetricsCollector& metrics);

bool step(
    TWRGraph& graph,
    LogicalStateStore& state_store,
    ExecutionTrace& trace,
    MetricsCollector& metrics);

void run_frame(
    TWRGraph& graph,
    LogicalStateStore& state_store,
    ExecutionTrace& trace,
    MetricsCollector& metrics);

void reset() noexcept;

The architectural scheduler uses priority, topological depth, and TWR ID as deterministic ordering keys.

## 9. SoftwareIncrementalScheduler

Header: include/twrf/core/software_incremental_scheduler.hpp

This is the executable Baseline C software incremental runtime.

size_t ready_set_size() const noexcept;
Timestamp current_step() const noexcept;

void prepare_frame(
    TWRGraph& graph,
    LogicalStateStore& state_store,
    ExecutionTrace& trace,
    MetricsCollector& metrics);

bool step(
    TWRGraph& graph,
    LogicalStateStore& state_store,
    ExecutionTrace& trace,
    MetricsCollector& metrics);

void run_frame(
    TWRGraph& graph,
    LogicalStateStore& state_store,
    ExecutionTrace& trace,
    MetricsCollector& metrics);

void reset() noexcept;

The software runtime deliberately uses explicit scanning and a software ready set instead of the architectural priority queue.

## 10. ExecutionTrace

Header: include/twrf/core/trace.hpp

void record_execution(...);
void record_skip(...);

const std::vector<TraceEntry>& entries() const noexcept;

std::vector<TWRId> executed_ids(Timestamp step) const;
std::vector<TWRId> executed_ids_since(size_t begin_index) const;

bool execution_set_equals(
    const ExecutionTrace& other,
    Timestamp step) const;

bool execution_set_equals_since(
    const ExecutionTrace& other,
    size_t begin_index,
    size_t other_begin_index) const;

void clear() noexcept;
std::string to_string() const;

The since variants are used for paired frame-scoped TWRF/Baseline C parity.

## 11. Raster renderer

Header: include/twrf/raster/renderer.hpp

TWRFRenderer(TileConfig config = TileConfig{});

const TileConfig& config() const noexcept;
Scene& scene() noexcept;
const Scene& scene() const noexcept;
const FrameBuffer& framebuffer() const noexcept;

LogicalStateStore& state_store() noexcept;
const ExecutionTrace& trace() const noexcept;
const ExecutionTrace& software_trace() const noexcept;
const MetricsCollector& metrics() const noexcept;
const MetricsCollector& software_metrics() const noexcept;
const LogicalStateStore& software_state_store() const noexcept;
const TWRGraph& graph() const noexcept;
TWRGraph& graph() noexcept;

void initialize();

void notify_camera_changed();
void notify_object_transform_changed(uint32_t object_id);
void notify_texture_changed(uint32_t texture_id);

RenderResult render_frame_incremental();
RenderResult render_frame_software_incremental();
FrameBuffer render_frame_forced_recompute();

void reset_measurement_metrics() noexcept;

RenderResult records:

frame_index
tiles_executed
tiles_skipped
skip_ratio
framebuffer
mutated_resources
executed_tiles
dirty_twr_count
state_store_reads
state_store_writes
state_store_read_bytes
state_store_write_bytes

The software incremental path has independent scheduler, State Store, trace, and metrics state.

## 12. Raster workload generator

Header: include/twrf/raster/workload_generator.hpp

LocalityPattern:
- Clustered
- Dispersed

setup_benchmark_scene(TWRFRenderer& renderer, int object_count = 16);

apply_change_step(
    TWRFRenderer& renderer,
    double p,
    int frame_step,
    LocalityPattern locality = LocalityPattern::Clustered);

p controls the requested object mutation fraction. Experiments separately derive object, dirty-region, and executed-region fractions.

## 13. RayBatchPipeline

Header: include/twrf/ray/ray_tracer.hpp

RayBatchPipeline(
    size_t batch_count = 16,
    size_t rays_per_batch = 64);

RayScene& scene() noexcept;
const RayScene& scene() const noexcept;
TWRGraph& graph() noexcept;
const TWRGraph& graph() const noexcept;
LogicalStateStore& state_store() noexcept;
const ExecutionTrace& trace() const noexcept;
const MetricsCollector& metrics() const noexcept;

void initialize_default_batches();
void notify_light_moved(const Vec3& new_pos);
void notify_geometry_changed();
void notify_camera_moved(const Vec3& new_pos);

uint64_t run_step();
std::vector<RayBatchPayload> trace_all_forced() const;
const std::vector<RayBatch>& batches() const noexcept;

Camera updates regenerate derived primary rays before subsequent ray TWR execution.

## 14. Neural types and TemporalMLPPipeline

Header: include/twrf/neural/tensor.hpp
Header: include/twrf/neural/mlp.hpp

Tensor1D:

Tensor1D();
explicit Tensor1D(size_t size, float init_val = 0.0f);
Tensor1D(std::initializer_list<float> list);
size_t size() const noexcept;
float& operator[](size_t i) noexcept;
const float& operator[](size_t i) const noexcept;
float dot(const Tensor1D& other) const noexcept;
Tensor1D operator+(const Tensor1D& other) const;
void relu_inplace() noexcept;
void gelu_inplace() noexcept;

Tensor2D:

Tensor2D();
Tensor2D(size_t rows, size_t cols, float init_val = 0.0f);
float& at(size_t row, size_t col) noexcept;
const float& at(size_t row, size_t col) const noexcept;
Tensor1D mat_vec(const Tensor1D& x) const;

MLPNetwork:

void add_layer(const MLPLayer& layer);
const std::vector<MLPLayer>& layers() const noexcept;
std::vector<MLPLayer>& layers() noexcept;
Tensor1D forward(const Tensor1D& input) const;
std::vector<uint8_t> serialize_weights() const;

TemporalMLPPipeline:

TemporalMLPPipeline(
    size_t input_dim = 4,
    size_t hidden_dim = 8,
    size_t output_dim = 4);

MLPNetwork& model() noexcept;
const MLPNetwork& model() const noexcept;
const Tensor1D& hidden_state() const noexcept;
TWRGraph& graph() noexcept;
LogicalStateStore& state_store() noexcept;
const MetricsCollector& metrics() const noexcept;

void initialize();
void set_input(const Tensor1D& in);
void notify_weights_changed();
uint64_t run_step();
Tensor1D get_current_output() const;

## 15. HeterogeneousPipeline

Header: include/twrf/api/heterogeneous_pipeline.hpp

HeterogeneousPipeline(size_t tile_count = 4);

TWRGraph& graph() noexcept;
LogicalStateStore& state_store() noexcept;
const MetricsCollector& metrics() const noexcept;
const ExecutionTrace& trace() const noexcept;

void initialize();
void notify_light_moved(const Vec3& new_pos);
void notify_geometry_moved(const Vec3& new_pos);
void notify_neural_weights_changed();

uint64_t run_frame();
uint64_t run_frame_forced_recompute();

ShadedTileOutput read_final_tile(size_t tile_idx) const;

Current graph:

Raster -> Ray
Raster -> Neural
Ray -> Neural

The direct Raster -> Neural edge is intentional because the neural stage reads the raster G-buffer directly.

## 16. Timing model

Headers:
include/twrf/timing/timing_params.hpp
include/twrf/timing/cycle_accounting.hpp
include/twrf/timing/baselines.hpp

Architectural cycle accounting is derived from measured simulator operation counts multiplied by configurable timing parameters.

Baseline A:
unconditional full recomputation.

Baseline B:
temporal-cache model with lookup/refill overhead.

Baseline C:
software incremental runtime with software timing parameters.

Derived cycles must not be described as physical GPU measurements.

## 17. ExperimentRunner

Header: include/twrf/timing/experiment_runner.hpp

run_change_rate_sweep(int frame_dim = 128, int tile_size = 16);
run_locality_sweep(int frame_dim = 128, int tile_size = 16);
run_full_matrix(int frame_dim = 128, int tile_size = 16);

export_json(
    const std::string& filepath,
    const std::vector<SweepDataPoint>& change_rate_data,
    const std::vector<SweepDataPoint>& locality_data,
    const std::vector<SweepDataPoint>* full_matrix = nullptr);

SweepDataPoint contains:

change_rate
locality
object_change_fraction
dirty_region_fraction
executed_region_fraction
tiles_executed
tiles_skipped
skip_ratio
twrf_cycles
baseline_a_cycles
baseline_b_cycles
baseline_c_cycles
execution_set_parity
output_parity
twrf_dependency_audit_failures
baseline_c_dependency_audit_failures

The standard full matrix contains 7 requested mutation rates x 2 locality patterns.

## 18. Research evidence boundary

Directly measured simulator quantities:
- execution/skip counts
- version checks
- bounding checks
- dependency traversals/propagations
- queue/ready-set operations
- State Store reads/writes and bytes
- audit outcomes
- execution traces
- output/workset parity.

Derived:
- cycle totals obtained from timing parameters.

Estimated:
- FPGA/RTL resource and timing feasibility until synthesis and hardware measurement exist.

This separation is part of the research protocol.
