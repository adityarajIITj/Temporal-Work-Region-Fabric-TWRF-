#include "twrf/raster/renderer.hpp"
#include "twrf/raster/workload_generator.hpp"
#include "tests/test_common.hpp"

struct RunSignature {
    uint64_t image_hash{0};
    uint64_t total_executions{0};
    uint64_t total_skips{0};
    std::string trace_dump;
};

RunSignature execute_scenario() {
    twrf::raster::TileConfig cfg;
    cfg.frame_width = 128;
    cfg.frame_height = 128;
    cfg.tile_size = 16;

    twrf::raster::TWRFRenderer renderer(cfg);
    twrf::raster::WorkloadGenerator::setup_benchmark_scene(renderer, 9);
    renderer.initialize();

    // 10-frame animation trajectory
    for (int f = 0; f < 10; ++f) {
        if (f % 2 == 1) {
            twrf::raster::WorkloadGenerator::apply_change_step(renderer, 0.25, f);
        }
        renderer.render_frame_incremental();
    }

    return RunSignature{
        renderer.framebuffer().compute_hash(),
        renderer.metrics().total_twr_executions,
        renderer.metrics().total_twr_skips,
        renderer.trace().to_string()
    };
}

int main() {
    TWRF_TEST_START("S2-07: Rasterizer Determinism & Repeatability Across Independent Runs");

    RunSignature run1 = execute_scenario();
    RunSignature run2 = execute_scenario();

    TWRF_ASSERT(run1.image_hash == run2.image_hash,
                "S2-07 Failed: Final framebuffer hash differs across identical runs");
    TWRF_ASSERT(run1.total_executions == run2.total_executions,
                "S2-07 Failed: Total executions count non-deterministic");
    TWRF_ASSERT(run1.total_skips == run2.total_skips,
                "S2-07 Failed: Total skips count non-deterministic");
    TWRF_ASSERT(run1.trace_dump == run2.trace_dump,
                "S2-07 Failed: Execution trace differs across identical runs");

    TWRF_TEST_PASS("S2-07: Rasterizer Determinism & Repeatability Across Independent Runs");
    return 0;
}
