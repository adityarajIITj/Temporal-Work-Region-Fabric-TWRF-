#pragma once

#include "twrf/timing/timing_params.hpp"
#include "twrf/timing/cycle_accounting.hpp"
#include "twrf/raster/renderer.hpp"
#include <cmath>
#include <vector>

namespace twrf::timing {

struct TileRecomputeWork {
    double compute_cycles{0.0};
    double memory_cycles{0.0};
};

class ArchitecturalModels {
public:
    static TileRecomputeWork compute_tile_work(const raster::TWRFRenderer& renderer, int tile_idx, const TimingParameters& params) {
        TileRecomputeWork w;
        const auto& cfg = renderer.config();
        const auto& sc = renderer.scene();
        twrf::BoundingRegion bounds = cfg.get_tile_bounds(tile_idx);
        int tile_pixels = cfg.tile_size * cfg.tile_size;

        for (const auto& obj : sc.objects) {
            if (bounds.overlaps(obj.screen_bounds)) {
                w.compute_cycles += obj.mesh.triangles().size() * params.cycles_per_triangle;
                w.compute_cycles += (tile_pixels * 0.5) * params.cycles_per_fragment;
                w.memory_cycles += (obj.mesh.triangles().size() * 36) * params.cycles_scene_dram_read_byte;
            }
        }
        return w;
    }

    // Model 1: Proposed TWRF Architecture
    static CycleAccounting evaluate_twrf(const raster::TWRFRenderer& renderer,
                                        const raster::RenderResult& render_res,
                                        const TimingParameters& params = TimingParameters::default_config()) {
        CycleAccounting acc;
        const auto& cfg = renderer.config();
        const auto& sc = renderer.scene();

        // Control-plane costs come directly from measured simulator counters.
        acc.change_detect_cycles +=
            (renderer.metrics().resource_version_checks +
             renderer.metrics().producer_version_checks) *
            params.cycles_version_check;
        acc.change_detect_cycles +=
            renderer.metrics().bounding_checks *
            params.cycles_bounding_check;

        acc.scheduler_cycles +=
            (renderer.metrics().ready_queue_pushes +
             renderer.metrics().ready_queue_pops) *
            params.cycles_queue_operation;

        acc.dependency_cycles +=
            renderer.metrics().dependency_traversals *
            params.cycles_dependency_notify;

        // 4. Compute cost (only for executed tiles)
        int tile_pixels = cfg.tile_size * cfg.tile_size;
        for (int i = 0; i < total_tiles; ++i) {
            bool executed = (i < static_cast<int>(render_res.executed_tiles.size()))
                                ? render_res.executed_tiles[i]
                                : false;

            if (executed) {
                TileRecomputeWork w = compute_tile_work(renderer, i, params);
                acc.compute_cycles += w.compute_cycles;
                acc.scene_memory_cycles += w.memory_cycles;

                // State Store traffic is accounted from measured per-frame
                // bytes below; keep this loop focused on compute and interconnect.

                // Logical NoC interconnect: Manhattan distance to center
                int tx = i % cfg.tiles_x();
                int ty = i / cfg.tiles_x();
                int cx = cfg.tiles_x() / 2;
                int cy = cfg.tiles_y() / 2;
                int manhattan_hops = std::abs(tx - cx) + std::abs(ty - cy);
                acc.interconnect_cycles += manhattan_hops * params.cycles_noc_hop;
            }
        }

        // Measured State Store traffic includes kernel commits and final
        // display composition reads for this frame.
        acc.state_store_cycles +=
            static_cast<double>(render_res.state_store_read_bytes) *
            params.cycles_state_store_read_byte;
        acc.state_store_cycles +=
            static_cast<double>(render_res.state_store_write_bytes) *
            params.cycles_state_store_write_byte;

        return acc;
    }

    // Model 2: Baseline A (Conventional SIMT / Full Recompute)
    static CycleAccounting evaluate_baseline_a_full_recompute(const raster::TWRFRenderer& renderer,
                                                            const TimingParameters& params = TimingParameters::default_config()) {
        CycleAccounting acc;
        const auto& cfg = renderer.config();
        int total_tiles = cfg.total_tiles();

        // Zero tracking overhead
        acc.change_detect_cycles = 0.0;
        acc.scheduler_cycles = 0.0;
        acc.dependency_cycles = 0.0;
        acc.state_store_cycles = 0.0;

        // Unconditionally recompute all tiles
        for (int i = 0; i < total_tiles; ++i) {
            TileRecomputeWork w = compute_tile_work(renderer, i, params);
            acc.compute_cycles += w.compute_cycles;
            acc.scene_memory_cycles += w.memory_cycles;
        }

        return acc;
    }

    // Model 3: Baseline C (Software Incremental Runtime)
    //
    // This is intentionally a management-cost model, not yet a second
    // renderer implementation. It uses the same measured execution set and
    // workload event trace as TWRF, then assigns software-side bookkeeping
    // costs to equivalent operations. It is therefore suitable for sensitivity
    // analysis, but must not be presented as measured software-runtime speedup.
    static CycleAccounting evaluate_baseline_c_software_incremental(
            const raster::TWRFRenderer& renderer,
            const raster::RenderResult& render_res,
            const TimingParameters& params = TimingParameters::default_config()) {
        CycleAccounting acc;
        const auto& cfg = renderer.config();
        const auto& sc = renderer.scene();
        const int total_tiles = cfg.total_tiles();
        const int tile_pixels = cfg.tile_size * cfg.tile_size;

        // Use measured B3 control-plane operation counts.
        acc.change_detect_cycles +=
            (renderer.software_metrics().resource_version_checks +
             renderer.software_metrics().producer_version_checks) *
            params.sw_cycles_version_check;
        acc.change_detect_cycles +=
            renderer.software_metrics().bounding_checks *
            params.sw_cycles_version_check;

        acc.scheduler_cycles +=
            (renderer.software_metrics().ready_queue_pushes +
             renderer.software_metrics().ready_queue_pops) *
            params.sw_cycles_queue_operation;

        acc.dependency_cycles +=
            renderer.software_metrics().dependency_traversals *
            params.sw_cycles_dependency_notify;

        for (int i = 0; i < total_tiles; ++i) {
            bool executed = (i < static_cast<int>(render_res.executed_tiles.size()))
                                ? render_res.executed_tiles[i] : false;
            if (!executed) continue;

            TileRecomputeWork w = compute_tile_work(renderer, i, params);
            acc.compute_cycles += w.compute_cycles;
            acc.scene_memory_cycles += w.memory_cycles;

        }

        // Per-frame State Store traffic is measured by the B3 runtime.
        acc.state_store_cycles +=
            static_cast<double>(render_res.state_store_read_bytes) *
            params.sw_cycles_state_store_read_byte;
        acc.state_store_cycles +=
            static_cast<double>(render_res.state_store_write_bytes) *
            params.sw_cycles_state_store_write_byte;

        return acc;
    }

    // Model 3: Baseline B (Conventional Temporal Cache / Software Reuse)
    static CycleAccounting evaluate_baseline_b_temporal_cache(const raster::TWRFRenderer& renderer,
                                                             const raster::RenderResult& render_res,
                                                             const TimingParameters& params = TimingParameters::default_config()) {
        CycleAccounting acc;
        const auto& cfg = renderer.config();
        int total_tiles = cfg.total_tiles();
        int tile_pixels = cfg.tile_size * cfg.tile_size;

        // Every tile incurs cache tag lookup
        acc.cache_overhead_cycles += total_tiles * params.cycles_cache_tag_lookup;

        uint64_t dirty_tiles = render_res.tiles_executed;
        uint64_t clean_tiles = render_res.tiles_skipped;

        // Cache misses (dirty tiles)
        acc.cache_overhead_cycles += dirty_tiles * (params.cycles_cache_miss_penalty + params.cycles_cache_eviction);

        // Cache hits (clean tiles) incur small read overhead
        acc.cache_overhead_cycles += clean_tiles * (tile_pixels * sizeof(raster::ColorRGBA) * 0.02);

        // Compute work executed on cache misses
        for (int i = 0; i < total_tiles; ++i) {
            bool executed = (i < static_cast<int>(render_res.executed_tiles.size()))
                                ? render_res.executed_tiles[i]
                                : false;

            if (executed) {
                TileRecomputeWork w = compute_tile_work(renderer, i, params);
                acc.compute_cycles += w.compute_cycles;
                acc.scene_memory_cycles += w.memory_cycles;
            }
        }

        return acc;
    }
};

} // namespace twrf::timing
