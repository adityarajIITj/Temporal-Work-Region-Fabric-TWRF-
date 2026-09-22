#include "twrf/timing/baselines.hpp"
#include "twrf/raster/workload_generator.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S3-05: 100% Dynamic Worst-Case & Tracking Overhead Penalty Transparency");

    twrf::raster::TileConfig cfg;
    cfg.frame_width = 128;
    cfg.frame_height = 128;
    cfg.tile_size = 16;

    twrf::raster::TWRFRenderer renderer(cfg);
    twrf::raster::WorkloadGenerator::setup_benchmark_scene(renderer, 16);
    renderer.initialize();

    // Warmup
    renderer.render_frame_incremental();

    // Apply 100% dynamic scene change (every single tile is dirtied / camera rotates)
    twrf::raster::WorkloadGenerator::apply_change_step(renderer, 1.0 /* 100% change */, 1);
    auto res = renderer.render_frame_incremental();

    auto twrf_acc = twrf::timing::ArchitecturalModels::evaluate_twrf(renderer, res);
    auto base_a_acc = twrf::timing::ArchitecturalModels::evaluate_baseline_a_full_recompute(renderer);

    // CRITICAL SCIENTIFIC INVARIANT (S3-05):
    // Under 100% dynamic scene motion (p = 1.0), TWRF performs equivalent recomputation
    // work PLUS tracking overhead Ct. Therefore, TWRF MUST strictly consume more cycles than Baseline A!
    // We explicitly assert and report this losing regime. DO NOT HIDE THIS RESULT!
    std::cout << "[INFO] S3-05 Measured Worst-Case Cycles:\n"
              << "       TWRF Total:       " << twrf_acc.total_cycles() << " cycles\n"
              << "       Baseline A Total: " << base_a_acc.total_cycles() << " cycles\n"
              << "       TWRF Penalty:     +" << (twrf_acc.total_cycles() - base_a_acc.total_cycles())
              << " cycles (tracking overhead tax)\n";

    TWRF_ASSERT(twrf_acc.total_cycles() > base_a_acc.total_cycles(),
                "S3-05 Failed: At 100% change, TWRF must strictly consume more cycles than Baseline A due to tracking overhead tax");
    TWRF_ASSERT(twrf_acc.tracking_overhead_cycles() > 0.0,
                "S3-05 Failed: Tracking overhead must be accounted for in worst case");

    TWRF_TEST_PASS("S3-05: 100% Dynamic Worst-Case & Tracking Overhead Penalty Transparency");
    return 0;
}
