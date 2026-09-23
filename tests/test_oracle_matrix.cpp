#include "twrf/raster/renderer.hpp"
#include "twrf/raster/workload_generator.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S4-02: Full-Recompute Oracle Parity Across Final Raster Matrix");

    const double rates[] = {0.00, 0.05, 0.10, 0.25, 0.50, 0.75, 1.00};
    const twrf::raster::LocalityPattern patterns[] = {
        twrf::raster::LocalityPattern::Clustered,
        twrf::raster::LocalityPattern::Dispersed
    };

    for (const auto locality : patterns) {
        for (const double p : rates) {
            twrf::raster::TileConfig cfg;
            cfg.frame_width = 128;
            cfg.frame_height = 128;
            cfg.tile_size = 16;

            twrf::raster::TWRFRenderer renderer(cfg);
            twrf::raster::WorkloadGenerator::setup_benchmark_scene(renderer, 16);
            renderer.initialize();

            renderer.render_frame_incremental();
            twrf::raster::WorkloadGenerator::apply_change_step(
                renderer, p, 1, locality);

            const auto incremental = renderer.render_frame_incremental();
            const auto oracle = renderer.render_frame_forced_recompute();

            TWRF_ASSERT(
                incremental.framebuffer.is_bitwise_identical(oracle),
                "S4-02: Incremental framebuffer differs from forced recompute oracle");
        }
    }

    TWRF_TEST_PASS("S4-02: Full-Recompute Oracle Parity Across Final Raster Matrix");
    return 0;
}
