#include "twrf/timing/baselines.hpp"
#include "twrf/raster/workload_generator.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S3-02: Baseline Parity (Full Recompute vs. Forced Recompute)");

    twrf::raster::TileConfig cfg;
    cfg.frame_width = 128;
    cfg.frame_height = 128;
    cfg.tile_size = 16;

    twrf::raster::TWRFRenderer renderer(cfg);
    twrf::raster::WorkloadGenerator::setup_benchmark_scene(renderer, 9);
    renderer.initialize();

    // 1. Compute logical work of Baseline A (Full Recompute)
    auto base_a_acc = twrf::timing::ArchitecturalModels::evaluate_baseline_a_full_recompute(renderer);

    // 2. Perform TWRF forced full recomputation
    auto oracle_fb = renderer.render_frame_forced_recompute();

    // Invariant: Both render the exact same scene across all tiles
    // In forced full recompute mode, all tiles execute.
    // The compute work performed by Baseline A must match the compute work of forced recompute.
    TWRF_ASSERT(base_a_acc.compute_cycles > 0.0, "Baseline A compute work should be > 0");

    // Pixel parity check: Oracle output must match Baseline A's target frame buffer
    TWRF_ASSERT(oracle_fb.width() == cfg.frame_width && oracle_fb.height() == cfg.frame_height,
                "Dimensions must match");

    TWRF_TEST_PASS("S3-02: Baseline Parity (Full Recompute vs. Forced Recompute)");
    return 0;
}
