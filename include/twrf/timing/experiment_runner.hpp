#pragma once

#include "twrf/timing/baselines.hpp"
#include "twrf/raster/workload_generator.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace twrf::timing {

struct SweepDataPoint {
    double change_rate{0.0};              // Requested object-mutation parameter p_o.
    std::string locality;
    double object_change_fraction{0.0};   // p_o
    double dirty_region_fraction{0.0};    // p_r
    double executed_region_fraction{0.0}; // p_e
    uint64_t tiles_executed{0};
    uint64_t tiles_skipped{0};
    double skip_ratio{0.0};

    CycleAccounting twrf_cycles;
    CycleAccounting baseline_a_cycles;
    CycleAccounting baseline_b_cycles;
    CycleAccounting baseline_c_cycles;

    bool execution_set_parity{false};
    bool output_parity{false};
    uint64_t twrf_dependency_audit_failures{0};
    uint64_t baseline_c_dependency_audit_failures{0};

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

struct SensitivityDataPoint {
    int tile_size{16};
    double hardware_control_multiplier{1.0};
    double state_store_latency_multiplier{1.0};
    uint64_t cases{0};
    uint64_t twrf_wins_vs_a{0};
    uint64_t twrf_wins_vs_b{0};
    uint64_t twrf_wins_vs_c{0};
    double twrf_vs_c_win_fraction{0.0};
    double min_twrf_vs_c_ratio{0.0};
    double max_twrf_vs_c_ratio{0.0};
};

class ExperimentRunner {
public:
    static std::vector<SweepDataPoint> run_change_rate_sweep(
            int frame_dim = 128, int tile_size = 16) {
        std::vector<SweepDataPoint> results;
        const double change_rates[] =
            {0.00, 0.05, 0.10, 0.25, 0.50, 0.75, 1.00};

        for (double p : change_rates) {
            results.push_back(run_single(frame_dim, tile_size, p,
                                         raster::LocalityPattern::Clustered,
                                         TimingParameters::default_config()));
        }
        return results;
    }

    static std::vector<SweepDataPoint> run_locality_sweep(
            int frame_dim = 128, int tile_size = 16) {
        std::vector<SweepDataPoint> results;
        results.push_back(run_single(frame_dim, tile_size, 0.25,
                                     raster::LocalityPattern::Clustered,
                                     TimingParameters::default_config()));
        results.push_back(run_single(frame_dim, tile_size, 0.25,
                                     raster::LocalityPattern::Dispersed,
                                     TimingParameters::default_config()));
        return results;
    }

    // Full p x locality matrix for the final experimental campaign.
    static std::vector<SweepDataPoint> run_full_matrix(
            int frame_dim = 128, int tile_size = 16) {
        return run_full_matrix(frame_dim, tile_size, TimingParameters::default_config());
    }

    static std::vector<SensitivityDataPoint> run_sensitivity_grid() {
        std::vector<SensitivityDataPoint> results;
        const int tile_sizes[] = {8, 16, 32};
        const double control_multipliers[] = {0.25, 0.50, 0.75, 1.00, 1.50, 2.00, 4.00, 8.00, 16.00};
        const double state_store_multipliers[] = {0.50, 1.00, 2.00, 4.00, 8.00};

        for (int tile_size : tile_sizes) {
            for (double control_multiplier : control_multipliers) {
                for (double state_multiplier : state_store_multipliers) {
                    TimingParameters params = TimingParameters::default_config();
                    params.cycles_version_check *= control_multiplier;
                    params.cycles_bounding_check *= control_multiplier;
                    params.cycles_queue_operation *= control_multiplier;
                    params.cycles_dependency_notify *= control_multiplier;
                    params.cycles_state_store_read_byte *= state_multiplier;
                    params.cycles_state_store_write_byte *= state_multiplier;

                    const auto matrix = run_full_matrix(128, tile_size, params);
                    SensitivityDataPoint point;
                    point.tile_size = tile_size;
                    point.hardware_control_multiplier = control_multiplier;
                    point.state_store_latency_multiplier = state_multiplier;
                    point.cases = static_cast<uint64_t>(matrix.size());

                    double min_ratio = 0.0;
                    double max_ratio = 0.0;
                    bool first_ratio = true;
                    for (const auto& row : matrix) {
                        point.twrf_wins_vs_a += row.twrf_wins_vs_a() ? 1U : 0U;
                        point.twrf_wins_vs_b += row.twrf_wins_vs_b() ? 1U : 0U;
                        point.twrf_wins_vs_c += row.twrf_wins_vs_c() ? 1U : 0U;
                        const double ratio =
                            safe_ratio(row.baseline_c_cycles.total_cycles(),
                                       row.twrf_cycles.total_cycles());
                        if (first_ratio) {
                            min_ratio = max_ratio = ratio;
                            first_ratio = false;
                        } else {
                            min_ratio = std::min(min_ratio, ratio);
                            max_ratio = std::max(max_ratio, ratio);
                        }
                    }

                    point.twrf_vs_c_win_fraction =
                        point.cases > 0
                            ? static_cast<double>(point.twrf_wins_vs_c) /
                                  static_cast<double>(point.cases)
                            : 0.0;
                    point.min_twrf_vs_c_ratio = min_ratio;
                    point.max_twrf_vs_c_ratio = max_ratio;
                    results.push_back(point);
                }
            }
        }
        return results;
    }

    static bool export_sensitivity_json(
            const std::string& filepath,
            const std::vector<SensitivityDataPoint>& data) {
        std::error_code ec;
        const std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path(), ec);
        }
        std::ofstream ofs(filepath);
        if (!ofs) return false;

        ofs << std::setprecision(12);
        ofs << "{\n  \"sensitivity\": [\n";
        for (size_t i = 0; i < data.size(); ++i) {
            const auto& point = data[i];
            ofs << "    {"
                << "\"tile_size\": " << point.tile_size
                << ", \"hardware_control_multiplier\": "
                << point.hardware_control_multiplier
                << ", \"state_store_latency_multiplier\": "
                << point.state_store_latency_multiplier
                << ", \"cases\": " << point.cases
                << ", \"twrf_wins_vs_a\": " << point.twrf_wins_vs_a
                << ", \"twrf_wins_vs_b\": " << point.twrf_wins_vs_b
                << ", \"twrf_wins_vs_c\": " << point.twrf_wins_vs_c
                << ", \"twrf_vs_c_win_fraction\": "
                << point.twrf_vs_c_win_fraction
                << ", \"min_twrf_vs_c_ratio\": "
                << point.min_twrf_vs_c_ratio
                << ", \"max_twrf_vs_c_ratio\": "
                << point.max_twrf_vs_c_ratio
                << "}" << (i + 1 < data.size() ? "," : "") << "\n";
        }
        ofs << "  ]\n}\n";
        return static_cast<bool>(ofs);
    }

    static std::vector<SweepDataPoint> run_full_matrix(
            int frame_dim, int tile_size, const TimingParameters& params) {
        std::vector<SweepDataPoint> results;
        const double change_rates[] =
            {0.00, 0.05, 0.10, 0.25, 0.50, 0.75, 1.00};
        const raster::LocalityPattern patterns[] = {
            raster::LocalityPattern::Clustered,
            raster::LocalityPattern::Dispersed
        };

        for (auto pattern : patterns) {
            for (double p : change_rates) {
                results.push_back(run_single(frame_dim, tile_size, p, pattern, params));
            }
        }
        return results;
    }

    static bool export_json(const std::string& filepath,
                            const std::vector<SweepDataPoint>& change_rate_data,
                            const std::vector<SweepDataPoint>& locality_data,
                            const std::vector<SweepDataPoint>* full_matrix = nullptr) {
        std::error_code ec;
        const std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path(), ec);
        }

        std::ofstream ofs(filepath);
        if (!ofs) {
            const std::filesystem::path fallback =
                std::filesystem::path("..") / filepath;
            if (fallback.has_parent_path()) {
                std::filesystem::create_directories(fallback.parent_path(), ec);
            }
            ofs.open(fallback);
            if (!ofs) return false;
        }

        ofs << std::setprecision(12);
        ofs << "{\n";
        write_array(ofs, "change_rate_sweep", change_rate_data);
        ofs << ",\n";
        write_array(ofs, "locality_sweep", locality_data);
        if (full_matrix) {
            ofs << ",\n";
            write_array(ofs, "full_matrix", *full_matrix);
        }
        ofs << "\n}\n";
        return static_cast<bool>(ofs);
    }

private:
    static SweepDataPoint run_single(int frame_dim, int tile_size, double p,
                                     raster::LocalityPattern locality,
                                     const TimingParameters& params =
                                         TimingParameters::default_config()) {
        raster::TileConfig cfg;
        cfg.frame_width = frame_dim;
        cfg.frame_height = frame_dim;
        cfg.tile_size = tile_size;

        // Two independent instances receive exactly the same scene and
        // mutation trace. Only the incremental execution engine differs.
        raster::TWRFRenderer twrf_renderer(cfg);
        raster::TWRFRenderer b3_renderer(cfg);
        raster::WorkloadGenerator::setup_benchmark_scene(twrf_renderer, 16);
        raster::WorkloadGenerator::setup_benchmark_scene(b3_renderer, 16);

        twrf_renderer.initialize();
        b3_renderer.initialize();

        // Identical cold-start/warmup.
        twrf_renderer.render_frame_incremental();
        b3_renderer.render_frame_software_incremental();
        twrf_renderer.reset_measurement_metrics();
        b3_renderer.reset_measurement_metrics();

        // Identical external event.
        raster::WorkloadGenerator::apply_change_step(
            twrf_renderer, p, 1, locality);
        raster::WorkloadGenerator::apply_change_step(
            b3_renderer, p, 1, locality);

        const size_t twrf_trace_start = twrf_renderer.trace().entries().size();
        const size_t b3_trace_start =
            b3_renderer.software_trace().entries().size();

        const auto twrf_res = twrf_renderer.render_frame_incremental();
        const auto b3_res = b3_renderer.render_frame_software_incremental();

        SweepDataPoint pt;
        pt.change_rate = p;
        pt.locality =
            locality == raster::LocalityPattern::Clustered
                ? "Clustered" : "Dispersed";

        const double object_count =
            static_cast<double>(twrf_renderer.scene().objects.size());
        const double region_count =
            static_cast<double>(twrf_renderer.config().total_tiles());

        pt.object_change_fraction =
            object_count > 0.0
                ? static_cast<double>(twrf_res.mutated_resources) / object_count
                : 0.0;
        pt.dirty_region_fraction =
            region_count > 0.0
                ? static_cast<double>(twrf_res.dirty_twr_count) / region_count
                : 0.0;
        pt.executed_region_fraction =
            region_count > 0.0
                ? static_cast<double>(twrf_res.tiles_executed) / region_count
                : 0.0;

        pt.tiles_executed = twrf_res.tiles_executed;
        pt.tiles_skipped = twrf_res.tiles_skipped;
        pt.skip_ratio = twrf_res.skip_ratio;

        pt.execution_set_parity =
            twrf_renderer.trace().execution_set_equals_since(
                b3_renderer.software_trace(),
                twrf_trace_start, b3_trace_start);
        pt.output_parity =
            twrf_res.framebuffer.is_bitwise_identical(b3_res.framebuffer);

        pt.twrf_dependency_audit_failures =
            twrf_renderer.metrics().dependency_audit_failures;
        pt.baseline_c_dependency_audit_failures =
            b3_renderer.software_metrics().dependency_audit_failures;

        // B1/B2 are evaluated against the TWRF measurement result. Because
        // Gate 3 requires execution-set parity, this is the same semantic
        // workset as B3 when the parity gate passes.
        pt.twrf_cycles =
            ArchitecturalModels::evaluate_twrf(twrf_renderer, twrf_res, params);
        pt.baseline_a_cycles =
            ArchitecturalModels::evaluate_baseline_a_full_recompute(twrf_renderer, params);
        pt.baseline_b_cycles =
            ArchitecturalModels::evaluate_baseline_b_temporal_cache(
                twrf_renderer, twrf_res, params);
        pt.baseline_c_cycles =
            ArchitecturalModels::evaluate_baseline_c_software_incremental(
                b3_renderer, b3_res, params);

        return pt;
    }

    static void write_array(std::ofstream& ofs, const char* key,
                            const std::vector<SweepDataPoint>& data) {
        ofs << "  \"" << key << "\": [\n";
        for (size_t i = 0; i < data.size(); ++i) {
            const auto& pt = data[i];
            ofs << "    {\n"
                << "      \"change_rate\": " << pt.change_rate << ",\n"
                << "      \"locality\": \"" << pt.locality << "\",\n"
                << "      \"object_change_fraction\": "
                << pt.object_change_fraction << ",\n"
                << "      \"dirty_region_fraction\": "
                << pt.dirty_region_fraction << ",\n"
                << "      \"executed_region_fraction\": "
                << pt.executed_region_fraction << ",\n"
                << "      \"tiles_executed\": " << pt.tiles_executed << ",\n"
                << "      \"tiles_skipped\": " << pt.tiles_skipped << ",\n"
                << "      \"skip_ratio\": " << pt.skip_ratio << ",\n"
                << "      \"execution_set_parity\": "
                << (pt.execution_set_parity ? "true" : "false") << ",\n"
                << "      \"output_parity\": "
                << (pt.output_parity ? "true" : "false") << ",\n"
                << "      \"twrf_dependency_audit_failures\": "
                << pt.twrf_dependency_audit_failures << ",\n"
                << "      \"baseline_c_dependency_audit_failures\": "
                << pt.baseline_c_dependency_audit_failures << ",\n"
                << "      \"twrf_total_cycles\": "
                << pt.twrf_cycles.total_cycles() << ",\n"
                << "      \"twrf_compute_cycles\": "
                << pt.twrf_cycles.compute_cycles << ",\n"
                << "      \"twrf_change_detect_cycles\": "
                << pt.twrf_cycles.change_detect_cycles << ",\n"
                << "      \"twrf_scheduler_cycles\": "
                << pt.twrf_cycles.scheduler_cycles << ",\n"
                << "      \"twrf_dependency_cycles\": "
                << pt.twrf_cycles.dependency_cycles << ",\n"
                << "      \"twrf_state_store_cycles\": "
                << pt.twrf_cycles.state_store_cycles << ",\n"
                << "      \"twrf_scene_memory_cycles\": "
                << pt.twrf_cycles.scene_memory_cycles << ",\n"
                << "      \"twrf_interconnect_cycles\": "
                << pt.twrf_cycles.interconnect_cycles << ",\n"
                << "      \"baseline_a_total_cycles\": "
                << pt.baseline_a_cycles.total_cycles() << ",\n"
                << "      \"baseline_b_total_cycles\": "
                << pt.baseline_b_cycles.total_cycles() << ",\n"
                << "      \"baseline_b_cache_overhead_cycles\": "
                << pt.baseline_b_cycles.cache_overhead_cycles << ",\n"
                << "      \"baseline_c_total_cycles\": "
                << pt.baseline_c_cycles.total_cycles() << ",\n"
                << "      \"baseline_c_compute_cycles\": "
                << pt.baseline_c_cycles.compute_cycles << ",\n"
                << "      \"baseline_c_change_detect_cycles\": "
                << pt.baseline_c_cycles.change_detect_cycles << ",\n"
                << "      \"baseline_c_scheduler_cycles\": "
                << pt.baseline_c_cycles.scheduler_cycles << ",\n"
                << "      \"baseline_c_dependency_cycles\": "
                << pt.baseline_c_cycles.dependency_cycles << ",\n"
                << "      \"baseline_c_state_store_cycles\": "
                << pt.baseline_c_cycles.state_store_cycles << ",\n"
                << "      \"baseline_c_scene_memory_cycles\": "
                << pt.baseline_c_cycles.scene_memory_cycles << ",\n"
                << "      \"twrf_vs_a_ratio\": "
                << safe_ratio(pt.baseline_a_cycles.total_cycles(),
                               pt.twrf_cycles.total_cycles()) << ",\n"
                << "      \"twrf_vs_b_ratio\": "
                << safe_ratio(pt.baseline_b_cycles.total_cycles(),
                               pt.twrf_cycles.total_cycles()) << ",\n"
                << "      \"twrf_vs_c_ratio\": "
                << safe_ratio(pt.baseline_c_cycles.total_cycles(),
                               pt.twrf_cycles.total_cycles()) << ",\n"
                << "      \"twrf_vs_a\": "
                << (pt.twrf_wins_vs_a() ? "true" : "false") << ",\n"
                << "      \"twrf_vs_b\": "
                << (pt.twrf_wins_vs_b() ? "true" : "false") << ",\n"
                << "      \"twrf_vs_c\": "
                << (pt.twrf_wins_vs_c() ? "true" : "false") << "\n"
                << "    }" << (i + 1 < data.size() ? "," : "") << "\n";
        }
        ofs << "  ]";
    }

    static double safe_ratio(double numerator, double denominator) noexcept {
        return denominator > 0.0 ? numerator / denominator : 0.0;
    }
};

} // namespace twrf::timing
