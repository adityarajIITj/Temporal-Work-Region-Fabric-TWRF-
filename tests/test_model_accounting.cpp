#include "twrf/timing/baselines.hpp"
#include "twrf/raster/workload_generator.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S3-01: Timing Model Component Attribution & Zero Untracked Cycles");

    twrf::raster::TileConfig cfg;
    cfg.frame_width = 128;
    cfg.frame_height = 128;
    cfg.tile_size = 16;

    twrf::raster::TWRFRenderer renderer(cfg);
    twrf::raster::WorkloadGenerator::setup_benchmark_scene(renderer, 9);
    renderer.initialize();

    // Warmup frame
    renderer.render_frame_incremental();

    // Measured frame: 25% change
    twrf::raster::WorkloadGenerator::apply_change_step(renderer, 0.25, 1);
    auto res = renderer.render_frame_incremental();

    auto twrf_acc = twrf::timing::ArchitecturalModels::evaluate_twrf(renderer, res);
    auto base_a_acc = twrf::timing::ArchitecturalModels::evaluate_baseline_a_full_recompute(renderer);
    auto base_b_acc = twrf::timing::ArchitecturalModels::evaluate_baseline_b_temporal_cache(renderer, res);

    // Invariant 1: In all models, total_cycles must equal the exact sum of all sub-components
    TWRF_ASSERT(twrf_acc.is_strictly_accounted(), "S3-01 Failed: TWRF model has untracked cycles");
    TWRF_ASSERT(base_a_acc.is_strictly_accounted(), "S3-01 Failed: Baseline A model has untracked cycles");
    TWRF_ASSERT(base_b_acc.is_strictly_accounted(), "S3-01 Failed: Baseline B model has untracked cycles");

    // Invariant 2: In TWRF, tracking overhead is non-zero
    TWRF_ASSERT(twrf_acc.tracking_overhead_cycles() > 0.0,
                "S3-01 Failed: TWRF tracking overhead must be explicitly modeled");

    // Invariant 3: In Baseline A, tracking overhead is strictly zero
    TWRF_ASSERT(base_a_acc.tracking_overhead_cycles() == 0.0,
                "S3-01 Failed: Baseline A should have zero tracking overhead");

    // Invariant 4: In Baseline B, cache overhead is non-zero
    TWRF_ASSERT(base_b_acc.cache_overhead_cycles > 0.0,
                "S3-01 Failed: Baseline B cache overhead must be explicitly modeled");

    TWRF_TEST_PASS("S3-01: Timing Model Component Attribution & Zero Untracked Cycles");
    return 0;
}
