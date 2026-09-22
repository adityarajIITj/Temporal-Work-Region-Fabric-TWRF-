#include "twrf/raster/renderer.hpp"
#include "twrf/raster/workload_generator.hpp"
#include "twrf/timing/baselines.hpp"
#include "twrf/timing/experiment_runner.hpp"
#include "twrf/api/heterogeneous_pipeline.hpp"
#include <iostream>
#include <iomanip>
#include <filesystem>

int main() {
    std::cout << "========================================================================\n"
              << "       TEMPORAL WORK REGION FABRIC (TWRF) VIRTUAL GPU SIMULATOR         \n"
              << "========================================================================\n\n";

    std::filesystem::create_directories("results");

    // -------------------------------------------------------------------------
    // DEMO 1: Functional 3D Rasterizer with Persistent Temporal Tile Reuse
    // -------------------------------------------------------------------------
    std::cout << "[DEMO 1] 3D Tiled Rasterizer & Temporal Reuse Pipeline\n"
              << "------------------------------------------------------------------------\n";
    twrf::raster::TileConfig cfg;
    cfg.frame_width = 128;
    cfg.frame_height = 128;
    cfg.tile_size = 16; // 8x8 = 64 tiles

    twrf::raster::TWRFRenderer renderer(cfg);
    twrf::raster::WorkloadGenerator::setup_benchmark_scene(renderer, 16);
    renderer.initialize();

    // Frame 0: Warmup
    std::cout << "  > Rendering Frame 0 (Cold Start / Full Initialization)...\n";
    auto r0 = renderer.render_frame_incremental();
    r0.framebuffer.save_ppm("results/demo_frame0.ppm");
    std::cout << "    - Tiles Executed: " << r0.tiles_executed << " / " << cfg.total_tiles() << "\n"
              << "    - Tiles Skipped:  " << r0.tiles_skipped << "\n"
              << "    - Saved PPM:      results/demo_frame0.ppm\n\n";

    // Frame 1: Static Scene (0% Volatility)
    std::cout << "  > Rendering Frame 1 (Static Scene / 0% Change)...\n";
    auto r1 = renderer.render_frame_incremental();
    r1.framebuffer.save_ppm("results/demo_frame1_static.ppm");
    std::cout << "    - Tiles Executed: " << r1.tiles_executed << " (Expected 0)\n"
              << "    - Tiles Skipped:  " << r1.tiles_skipped << " (100% temporal reuse)\n"
              << "    - Skip Ratio:     " << std::fixed << std::setprecision(1) << (r1.skip_ratio * 100.0) << "%\n"
              << "    - Saved PPM:      results/demo_frame1_static.ppm\n\n";

    // Frame 2: Moving Object (Localized Invalidation)
    std::cout << "  > Rendering Frame 2 (Single Object Mutation / Localized Invalidation)...\n";
    twrf::raster::WorkloadGenerator::apply_change_step(renderer, 0.05, 1);
    auto r2 = renderer.render_frame_incremental();
    r2.framebuffer.save_ppm("results/demo_frame2_dynamic.ppm");
    std::cout << "    - Tiles Executed: " << r2.tiles_executed << " (Only conservatively affected tiles)\n"
              << "    - Tiles Skipped:  " << r2.tiles_skipped << " (Clean background/unaffected tiles)\n"
              << "    - Skip Ratio:     " << (r2.skip_ratio * 100.0) << "%\n"
              << "    - Saved PPM:      results/demo_frame2_dynamic.ppm\n\n";

    // -------------------------------------------------------------------------
    // DEMO 2: Heterogeneous Cross-Workload Pipeline (Raster -> Ray -> Neural)
    // -------------------------------------------------------------------------
    std::cout << "[DEMO 2] Heterogeneous Rendering DAG (Raster GBuffer -> Ray Shadow -> Neural)\n"
              << "------------------------------------------------------------------------\n";
    twrf::api::HeterogeneousPipeline het_pipe(4 /* 4 tiles */);
    het_pipe.initialize();

    std::cout << "  > Executing Heterogeneous Cold Start...\n";
    uint64_t h_exec1 = het_pipe.run_frame();
    std::cout << "    - Executed: " << h_exec1 << " / 12 heterogeneous TWRs (4 Raster + 4 Ray + 4 Neural)\n";

    std::cout << "  > Moving Light Source (Spatial Occlusion Mutation)...\n";
    het_pipe.notify_light_moved(twrf::raster::Vec3(10.0f, 15.0f, 5.0f));
    uint64_t h_exec2 = het_pipe.run_frame();
    std::cout << "    - Executed: " << h_exec2 << " TWRs (Raster skipped: 4 clean; Ray & Neural executed: 8)\n";

    std::cout << "  > Static Heterogeneous Step...\n";
    uint64_t h_exec3 = het_pipe.run_frame();
    std::cout << "    - Executed: " << h_exec3 << " TWRs (100% cross-stage temporal reuse)\n\n";

    // -------------------------------------------------------------------------
    // DEMO 3: Architectural Timing Model & Break-Even Evaluation
    // -------------------------------------------------------------------------
    std::cout << "[DEMO 3] Architectural Break-Even Analysis Across 7 Volatility Regimes\n"
              << "------------------------------------------------------------------------\n";
    auto change_data = twrf::timing::ExperimentRunner::run_change_rate_sweep(128, 16);
    auto locality_data = twrf::timing::ExperimentRunner::run_locality_sweep(128, 16);
    twrf::timing::ExperimentRunner::export_json("results/phase3_sweeps.json", change_data, locality_data);

    std::cout << std::setw(15) << "Change Rate (p)"
              << std::setw(16) << "TWRF (Cycles)"
              << std::setw(20) << "Baseline A (Full)"
              << std::setw(20) << "Baseline B (Cache)"
              << std::setw(18) << "TWRF Verdict" << "\n";
    std::cout << std::string(89, '-') << "\n";

    for (const auto& pt : change_data) {
        std::string verdict;
        if (pt.twrf_wins_vs_a()) {
            double speedup = pt.baseline_a_cycles.total_cycles() / pt.twrf_cycles.total_cycles();
            std::ostringstream ss;
            ss << "WIN (" << std::fixed << std::setprecision(2) << speedup << "x)";
            verdict = ss.str();
        } else {
            double tax = (pt.twrf_cycles.total_cycles() - pt.baseline_a_cycles.total_cycles()) / pt.baseline_a_cycles.total_cycles() * 100.0;
            std::ostringstream ss;
            ss << "LOSS (+" << std::fixed << std::setprecision(1) << tax << "%)";
            verdict = ss.str();
        }

        std::cout << std::setw(14) << (std::to_string(static_cast<int>(pt.change_rate * 100)) + "%")
                  << std::setw(16) << std::fixed << std::setprecision(1) << pt.twrf_cycles.total_cycles()
                  << std::setw(20) << pt.baseline_a_cycles.total_cycles()
                  << std::setw(20) << pt.baseline_b_cycles.total_cycles()
                  << std::setw(18) << verdict << "\n";
    }

    std::cout << "------------------------------------------------------------------------\n"
              << "[INFO] Machine-readable experimental results saved to: results/phase3_sweeps.json\n"
              << "[INFO] Run 'python python/analysis/plot_break_even.py' to view the graphical curve.\n\n"
              << "========================================================================\n"
              << "                     TWRF DEMONSTRATION COMPLETE                        \n"
              << "========================================================================\n";

    return 0;
}
