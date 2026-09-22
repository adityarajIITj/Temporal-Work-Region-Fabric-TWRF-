#include "twrf/raster/renderer.hpp"
#include "twrf/raster/workload_generator.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S2-05: 100% Dynamic Scene Fallback & Dynamics Sweep");

    twrf::raster::TileConfig cfg;
    cfg.frame_width = 128;
    cfg.frame_height = 128;
    cfg.tile_size = 16;

    twrf::raster::TWRFRenderer renderer(cfg);
    twrf::raster::WorkloadGenerator::setup_benchmark_scene(renderer, 9);
    renderer.initialize();

    // Initial frame
    renderer.render_frame_incremental();

    // 100% Dynamic test: All objects or camera continuously move
    for (int step = 1; step <= 5; ++step) {
        twrf::raster::WorkloadGenerator::apply_change_step(renderer, 1.0 /* 100% change */, step);

        auto res = renderer.render_frame_incremental();
        auto oracle = renderer.render_frame_forced_recompute();

        TWRF_ASSERT(res.framebuffer.is_bitwise_identical(oracle),
                    "S2-05 Failed: 100% dynamic frame output does not match oracle");
        TWRF_ASSERT(res.tiles_executed > 0,
                    "S2-05 Failed: Tiles must execute on 100% dynamic workload");
    }

    // Controlled change rate sweep: 0%, 25%, 50%, 75%
    double change_rates[] = {0.0, 0.25, 0.50, 0.75};
    for (double p : change_rates) {
        twrf::raster::WorkloadGenerator::apply_change_step(renderer, p, 10);
        auto res = renderer.render_frame_incremental();
        auto oracle = renderer.render_frame_forced_recompute();

        TWRF_ASSERT(res.framebuffer.is_bitwise_identical(oracle),
                    "S2-05 Failed: Dynamic scene sweep output does not match oracle");
    }

    TWRF_TEST_PASS("S2-05: 100% Dynamic Scene Fallback & Dynamics Sweep");
    return 0;
}
