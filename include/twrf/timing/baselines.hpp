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

        // 1. Change detection cost (paid by all tiles to check inputs)
        int total_tiles = cfg.total_tiles();
        // Version check is evaluated for all bound input resources across all tiles
        acc.change_detect_cycles += params.cycles_version_check * (1 + sc.objects.size()) * total_tiles;
        // Bounding check is only performed for mutated resources requiring spatial intersection testing
        uint64_t checked_mutations = (render_res.mutated_resources > 0)
                                         ? std::min(render_res.mutated_resources, static_cast<uint64_t>(sc.objects.size()))
                                         : (render_res.tiles_executed > 0 ? static_cast<uint64_t>(sc.objects.size()) : 0);
        acc.change_detect_cycles += params.cycles_bounding_check * checked_mutations * total_tiles;

        // 2. Scheduler cost
        acc.scheduler_cycles += render_res.tiles_executed * params.cycles_queue_operation * 2.0;

        // 3. Dependency propagation
        acc.dependency_cycles += renderer.metrics().dependency_traversals * params.cycles_dependency_notify;

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

                // Write output to State Store
                size_t payload_bytes = tile_pixels * (sizeof(raster::ColorRGBA) + sizeof(float));
                acc.state_store_cycles += payload_bytes * params.cycles_state_store_write_byte;

                // Logical NoC interconnect: Manhattan distance to center
                int tx = i % cfg.tiles_x();
                int ty = i / cfg.tiles_x();
                int cx = cfg.tiles_x() / 2;
                int cy = cfg.tiles_y() / 2;
                int manhattan_hops = std::abs(tx - cx) + std::abs(ty - cy);
                acc.interconnect_cycles += manhattan_hops * params.cycles_noc_hop;
            }
        }

        // 5. Read state store for display composition (all tiles)
        size_t display_read_bytes = tile_pixels * sizeof(raster::ColorRGBA);
        acc.state_store_cycles += total_tiles * display_read_bytes * params.cycles_state_store_read_byte;

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

        // Software scans the same resource bindings to detect version changes.
        acc.change_detect_cycles +=
            params.sw_cycles_version_check * (1 + sc.objects.size()) * total_tiles;

        // Explicit ready-set bookkeeping.
        acc.scheduler_cycles +=
            render_res.tiles_executed * params.sw_cycles_queue_operation * 2.0;

        // Software dependency traversal/notification.
        acc.dependency_cycles +=
            renderer.metrics().dependency_traversals *
            params.sw_cycles_dependency_notify;

        for (int i = 0; i < total_tiles; ++i) {
            bool executed = (i < static_cast<int>(render_res.executed_tiles.size()))
                                ? render_res.executed_tiles[i] : false;
            if (!executed) continue;

            TileRecomputeWork w = compute_tile_work(renderer, i, params);
            acc.compute_cycles += w.compute_cycles;
            acc.scene_memory_cycles += w.memory_cycles;

            size_t payload_bytes =
                static_cast<size_t>(tile_pixels) *
                (sizeof(raster::ColorRGBA) + sizeof(float));
            acc.state_store_cycles +=
                payload_bytes * params.sw_cycles_state_store_write_byte;
        }

        // Persistent software cache/state is still read for display.
        size_t display_read_bytes =
            static_cast<size_t>(tile_pixels) * sizeof(raster::ColorRGBA);
        acc.state_store_cycles +=
            static_cast<double>(total_tiles) * display_read_bytes *
            params.sw_cycles_state_store_read_byte;

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
