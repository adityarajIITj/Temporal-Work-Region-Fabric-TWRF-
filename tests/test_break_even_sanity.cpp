#include "twrf/timing/baselines.hpp"
#include "twrf/raster/workload_generator.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S3-04: Break-Even Monotonicity Under Tracking Overhead Sweeps");

    twrf::raster::TileConfig cfg;
    cfg.frame_width = 128;
    cfg.frame_height = 128;
    cfg.tile_size = 16;

    twrf::raster::TWRFRenderer renderer(cfg);
    twrf::raster::WorkloadGenerator::setup_benchmark_scene(renderer, 9);
    renderer.initialize();

    renderer.render_frame_incremental();
    twrf::raster::WorkloadGenerator::apply_change_step(renderer, 0.25, 1);
    auto res = renderer.render_frame_incremental();

    // Test three tracking cost parameter profiles: Low, Medium, High
    twrf::timing::TimingParameters p_low = twrf::timing::TimingParameters::default_config();

    twrf::timing::TimingParameters p_med = p_low;
    p_med.cycles_version_check *= 5.0;
    p_med.cycles_bounding_check *= 5.0;
    p_med.cycles_queue_operation *= 5.0;

    twrf::timing::TimingParameters p_high = p_low;
    p_high.cycles_version_check *= 25.0;
    p_high.cycles_bounding_check *= 25.0;
    p_high.cycles_queue_operation *= 25.0;

    auto acc_low = twrf::timing::ArchitecturalModels::evaluate_twrf(renderer, res, p_low);
    auto acc_med = twrf::timing::ArchitecturalModels::evaluate_twrf(renderer, res, p_med);
    auto acc_high = twrf::timing::ArchitecturalModels::evaluate_twrf(renderer, res, p_high);

    // Invariant 1: Monotonicity of tracking overhead
    TWRF_ASSERT(acc_low.tracking_overhead_cycles() < acc_med.tracking_overhead_cycles(),
                "S3-04 Failed: Medium tracking overhead must exceed low overhead");
    TWRF_ASSERT(acc_med.tracking_overhead_cycles() < acc_high.tracking_overhead_cycles(),
                "S3-04 Failed: High tracking overhead must exceed medium overhead");

    // Invariant 2: Total cycles monotonically increase as tracking cost rises
    TWRF_ASSERT(acc_low.total_cycles() < acc_med.total_cycles(),
                "S3-04 Failed: Total cycles must monotonically increase with Ct");
    TWRF_ASSERT(acc_med.total_cycles() < acc_high.total_cycles(),
                "S3-04 Failed: Total cycles must monotonically increase with Ct");

    // Invariant 3: Recomputation compute_cycles remain constant across tracking cost sweeps
    TWRF_ASSERT(std::abs(acc_low.compute_cycles - acc_high.compute_cycles) < 1e-6,
                "S3-04 Failed: Recompute work Cr must not be influenced by Ct parameters");

    TWRF_TEST_PASS("S3-04: Break-Even Monotonicity Under Tracking Overhead Sweeps");
    return 0;
}
