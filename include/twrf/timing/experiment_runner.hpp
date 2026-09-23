#pragma once

#include "twrf/timing/baselines.hpp"
#include "twrf/raster/workload_generator.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <filesystem>

namespace twrf::timing {

struct SweepDataPoint {
    double change_rate{0.0};
    std::string locality;
    uint64_t tiles_executed{0};
    uint64_t tiles_skipped{0};
    double skip_ratio{0.0};

    CycleAccounting twrf_cycles;
    CycleAccounting baseline_a_cycles;
    CycleAccounting baseline_b_cycles;
    CycleAccounting baseline_c_cycles;

    [[nodiscard]] bool twrf_wins_vs_a() const noexcept {
        return twrf_cycles.total_cycles() < baseline_a_cycles.total_cycles();
    }

    [[nodiscard]] bool twrf_wins_vs_b() const noexcept {
        return twrf_cycles.total_cycles() < baseline_b_cycles.total_cycles();
    }

    [[nodiscard]] bool twrf_wins_vs_c() const noexcept {
        return twrf_cycles.total_cycles() < baseline_c_cycles.total_cycles();
    }
};

class ExperimentRunner {
public:
    static std::vector<SweepDataPoint> run_change_rate_sweep(int frame_dim = 128, int tile_size = 16) {
        std::vector<SweepDataPoint> results;
        double change_rates[] = {0.00, 0.05, 0.10, 0.25, 0.50, 0.75, 1.00};

        raster::TileConfig cfg;
        cfg.frame_width = frame_dim;
        cfg.frame_height = frame_dim;
        cfg.tile_size = tile_size;

        for (double p : change_rates) {
            raster::TWRFRenderer renderer(cfg);
            raster::WorkloadGenerator::setup_benchmark_scene(renderer, 16);
            renderer.initialize();

            // Warmup frame
            renderer.render_frame_incremental();

            // Measurement frame with change rate p
            raster::WorkloadGenerator::apply_change_step(renderer, p, 1, raster::LocalityPattern::Clustered);
            auto res = renderer.render_frame_incremental();

            SweepDataPoint pt;
            pt.change_rate = p;
            pt.locality = "Clustered";
            pt.tiles_executed = res.tiles_executed;
            pt.tiles_skipped = res.tiles_skipped;
            pt.skip_ratio = res.skip_ratio;

            pt.twrf_cycles = ArchitecturalModels::evaluate_twrf(renderer, res);
            pt.baseline_a_cycles = ArchitecturalModels::evaluate_baseline_a_full_recompute(renderer);
            pt.baseline_b_cycles = ArchitecturalModels::evaluate_baseline_b_temporal_cache(renderer, res);
            pt.baseline_c_cycles = ArchitecturalModels::evaluate_baseline_c_software_incremental(renderer, res);

            results.push_back(pt);
        }

        return results;
    }

    static std::vector<SweepDataPoint> run_locality_sweep(int frame_dim = 128, int tile_size = 16) {
        std::vector<SweepDataPoint> results;
        raster::LocalityPattern patterns[] = {raster::LocalityPattern::Clustered, raster::LocalityPattern::Dispersed};

        raster::TileConfig cfg;
        cfg.frame_width = frame_dim;
        cfg.frame_height = frame_dim;
        cfg.tile_size = tile_size;

        for (auto loc : patterns) {
            raster::TWRFRenderer renderer(cfg);
            raster::WorkloadGenerator::setup_benchmark_scene(renderer, 16);
            renderer.initialize();

            renderer.render_frame_incremental();
            raster::WorkloadGenerator::apply_change_step(renderer, 0.25, 1, loc);
            auto res = renderer.render_frame_incremental();

            SweepDataPoint pt;
            pt.change_rate = 0.25;
            pt.locality = (loc == raster::LocalityPattern::Clustered) ? "Clustered" : "Dispersed";
            pt.tiles_executed = res.tiles_executed;
            pt.tiles_skipped = res.tiles_skipped;
            pt.skip_ratio = res.skip_ratio;

            pt.twrf_cycles = ArchitecturalModels::evaluate_twrf(renderer, res);
            pt.baseline_a_cycles = ArchitecturalModels::evaluate_baseline_a_full_recompute(renderer);
            pt.baseline_b_cycles = ArchitecturalModels::evaluate_baseline_b_temporal_cache(renderer, res);
            pt.baseline_c_cycles = ArchitecturalModels::evaluate_baseline_c_software_incremental(renderer, res);

            results.push_back(pt);
        }

        return results;
    }

    static bool export_json(const std::string& filepath,
                            const std::vector<SweepDataPoint>& change_rate_data,
                            const std::vector<SweepDataPoint>& locality_data) {
        std::error_code ec;
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path(), ec);
        }

        std::ofstream ofs(filepath);
        if (!ofs) {
            std::filesystem::path fallback = std::filesystem::path("..") / filepath;
            if (fallback.has_parent_path()) {
                std::filesystem::create_directories(fallback.parent_path(), ec);
            }
            ofs.open(fallback);
            if (!ofs) return false;
        }

        ofs << "{\n";
        ofs << "  \"change_rate_sweep\": [\n";
        for (size_t i = 0; i < change_rate_data.size(); ++i) {
            const auto& pt = change_rate_data[i];
            ofs << "    {\n"
                << "      \"change_rate\": " << pt.change_rate << ",\n"
                << "      \"locality\": \"" << pt.locality << "\",\n"
                << "      \"tiles_executed\": " << pt.tiles_executed << ",\n"
                << "      \"tiles_skipped\": " << pt.tiles_skipped << ",\n"
                << "      \"skip_ratio\": " << pt.skip_ratio << ",\n"
                << "      \"twrf_total_cycles\": " << pt.twrf_cycles.total_cycles() << ",\n"
                << "      \"baseline_a_total_cycles\": " << pt.baseline_a_cycles.total_cycles() << ",\n"
                << "      \"baseline_b_total_cycles\": " << pt.baseline_b_cycles.total_cycles() << ",\n"
                << "      \"twrf_wins_vs_a\": " << (pt.twrf_wins_vs_a() ? "true" : "false") << ",\n"
                << "      \"twrf_wins_vs_b\": " << (pt.twrf_wins_vs_b() ? "true" : "false") << "\n"
                << "    }" << (i + 1 < change_rate_data.size() ? "," : "") << "\n";
        }
        ofs << "  ],\n";

        ofs << "  \"locality_sweep\": [\n";
        for (size_t i = 0; i < locality_data.size(); ++i) {
            const auto& pt = locality_data[i];
            ofs << "    {\n"
                << "      \"change_rate\": " << pt.change_rate << ",\n"
                << "      \"locality\": \"" << pt.locality << "\",\n"
                << "      \"tiles_executed\": " << pt.tiles_executed << ",\n"
                << "      \"tiles_skipped\": " << pt.tiles_skipped << ",\n"
                << "      \"skip_ratio\": " << pt.skip_ratio << ",\n"
                << "      \"twrf_total_cycles\": " << pt.twrf_cycles.total_cycles() << ",\n"
                << "      \"baseline_a_total_cycles\": " << pt.baseline_a_cycles.total_cycles() << ",\n"
                << "      \"baseline_b_total_cycles\": " << pt.baseline_b_cycles.total_cycles() << ",\n                << "      \"baseline_c_total_cycles\": " << pt.baseline_c_cycles.total_cycles() << "\n"
                << "    }" << (i + 1 < locality_data.size() ? "," : "") << "\n";
        }
        ofs << "  ]\n";
        ofs << "}\n";

        return true;
    }
};

} // namespace twrf::timing
