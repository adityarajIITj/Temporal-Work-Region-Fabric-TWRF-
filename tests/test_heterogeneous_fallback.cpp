#include "twrf/api/heterogeneous_pipeline.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S4-05: Heterogeneous Fallback & Incremental Output Parity");

    // Pipeline 1: Incremental mode (warmup -> dynamic change -> incremental render)
    twrf::api::HeterogeneousPipeline inc_pipe(4);
    inc_pipe.initialize();
    inc_pipe.run_frame(); // Frame 1: Warmup

    inc_pipe.notify_light_moved(twrf::raster::Vec3(3.0f, 6.0f, 1.0f));
    uint64_t inc_execs = inc_pipe.run_frame(); // Frame 2: Incremental reuse (Raster skipped, 8 execs)
    TWRF_ASSERT(inc_execs == 8, "S4-05 Failed: Incremental execution must reuse 4 clean raster tiles");

    // Pipeline 2: Full recompute fallback mode with identical initial history
    twrf::api::HeterogeneousPipeline full_pipe(4);
    full_pipe.initialize();
    full_pipe.run_frame(); // Frame 1: Warmup

    full_pipe.notify_light_moved(twrf::raster::Vec3(3.0f, 6.0f, 1.0f));
    uint64_t full_execs = full_pipe.run_frame_forced_recompute(); // Frame 2: Forced full recompute (all 12 execs)
    TWRF_ASSERT(full_execs == 12, "S4-05 Failed: Forced recompute mode must execute all 12 TWRs");

    // Invariant: Outputs of both pipelines across all 4 tiles must be numerically identical
    for (size_t t = 0; t < 4; ++t) {
        auto inc_tile = inc_pipe.read_final_tile(t);
        auto full_tile = full_pipe.read_final_tile(t);

        float color_diff = (inc_tile.final_color - full_tile.final_color).length();
        float conf_diff = std::abs(inc_tile.temporal_confidence - full_tile.temporal_confidence);

        TWRF_ASSERT(color_diff < 1e-4f,
                    "S4-05 Failed: Incremental shaded color differs from full recompute oracle");
        TWRF_ASSERT(conf_diff < 1e-4f,
                    "S4-05 Failed: Incremental temporal confidence differs from full recompute oracle");
    }

    TWRF_TEST_PASS("S4-05: Heterogeneous Fallback & Incremental Output Parity");
    return 0;
}
