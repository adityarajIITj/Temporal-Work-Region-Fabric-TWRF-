#include "twrf/raster/renderer.hpp"
#include "twrf/raster/workload_generator.hpp"
#include "twrf/timing/experiment_runner.hpp"
#include "twrf/api/heterogeneous_pipeline.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    std::cout
        << "========================================================================\n"
        << "       TEMPORAL WORK REGION FABRIC (TWRF) VIRTUAL GPU SIMULATOR       \n"
        << "========================================================================\n\n";

    std::filesystem::create_directories("results");

    std::cout
        << "[DEMO 1] Tiled Raster + Temporal Reuse\n"
        << "------------------------------------------------------------------------\n";

    twrf::raster::TileConfig cfg;
    cfg.frame_width = 128;
    cfg.frame_height = 128;
    cfg.tile_size = 16;

    twrf::raster::TWRFRenderer renderer(cfg);
    twrf::raster::WorkloadGenerator::setup_benchmark_scene(renderer, 16);
    renderer.initialize();

    const auto r0 = renderer.render_frame_incremental();
    std::cout << "Cold frame: executed " << r0.tiles_executed
              << " / " << cfg.total_tiles() << " tiles\n";

    const auto r1 = renderer.render_frame_incremental();
    std::cout << "Static frame: executed " << r1.tiles_executed
              << ", skipped " << r1.tiles_skipped << "\n";

    twrf::raster::WorkloadGenerator::apply_change_step(
        renderer, 0.05, 1, twrf::raster::LocalityPattern::Clustered);
    const auto r2 = renderer.render_frame_incremental();
    std::cout << "Localized mutation: executed " << r2.tiles_executed
              << ", skipped " << r2.tiles_skipped
              << ", p_e=" << std::fixed << std::setprecision(3)
              << (static_cast<double>(r2.tiles_executed) / cfg.total_tiles()) << "\n\n";

    std::cout
        << "[DEMO 2] Heterogeneous Raster -> Ray -> Neural DAG\n"
        << "------------------------------------------------------------------------\n";

    twrf::api::HeterogeneousPipeline het_pipe(4);
    het_pipe.initialize();

    const uint64_t h0 = het_pipe.run_frame();
    std::cout << "Cold heterogeneous frame: " << h0 << " TWRs executed\n";

    het_pipe.notify_light_moved(twrf::raster::Vec3(10.0f, 15.0f, 5.0f));
    const uint64_t h1 = het_pipe.run_frame();
    std::cout << "Light mutation: " << h1
              << " TWRs executed; raster remains dependency-disjoint from light\n";

    const uint64_t h2 = het_pipe.run_frame();
    std::cout << "Static heterogeneous frame: " << h2 << " TWRs executed\n\n";

    std::cout
        << "[DEMO 3] Final Experimental Matrix\n"
        << "------------------------------------------------------------------------\n";

    const auto change_data =
        twrf::timing::ExperimentRunner::run_change_rate_sweep(128, 16);
    const auto locality_data =
        twrf::timing::ExperimentRunner::run_locality_sweep(128, 16);
    const auto full_matrix =
        twrf::timing::ExperimentRunner::run_full_matrix(128, 16);

    const bool exported = twrf::timing::ExperimentRunner::export_json(
        "results/phase3_sweeps.json", change_data, locality_data, &full_matrix);
    if (!exported) {
        std::cerr << "[ERROR] Failed to export experimental results.\n";
        return 2;
    }

    const auto sensitivity_data =
        twrf::timing::ExperimentRunner::run_sensitivity_grid();
    const bool sensitivity_exported =
        twrf::timing::ExperimentRunner::export_sensitivity_json(
            "results/twrf_sensitivity.json", sensitivity_data);
    if (!sensitivity_exported) {
        std::cerr << "[ERROR] Failed to export sensitivity results.\n";
        return 2;
    }

    size_t parity_failures = 0;
    size_t output_failures = 0;
    size_t audit_failures = 0;
    size_t b3_audit_failures = 0;

    for (const auto& pt : full_matrix) {
        parity_failures += pt.execution_set_parity ? 0 : 1;
        output_failures += pt.output_parity ? 0 : 1;
        audit_failures += static_cast<size_t>(pt.twrf_dependency_audit_failures);
        b3_audit_failures +=
            static_cast<size_t>(pt.baseline_c_dependency_audit_failures);
    }

    std::cout << "Cases: " << full_matrix.size() << " (7 change rates x 2 localities)\n"
              << "Execution-set parity failures: " << parity_failures << "\n"
              << "Output parity failures: " << output_failures << "\n"
              << "TWRF audit failures: " << audit_failures << "\n"
              << "B3 audit failures: " << b3_audit_failures << "\n";

    if (parity_failures || output_failures || audit_failures ||
        b3_audit_failures) {
        std::cerr << "[ERROR] Research parity/semantic gates failed.\n";
        return 3;
    }

    std::cout
        << "Sensitivity settings: " << sensitivity_data.size() << "\n"
        << "Machine-readable results: results/phase3_sweeps.json\n"
        << "Sensitivity results: results/twrf_sensitivity.json\n"
        << "========================================================================\n"
        << "                     TWRF DEMONSTRATION COMPLETE                      \n"
        << "========================================================================\n";

    return 0;
}
