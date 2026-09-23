#pragma once

#include "twrf/core/types.hpp"
#include "twrf/core/graph.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/state_store.hpp"
#include "twrf/core/trace.hpp"
#include "twrf/core/metrics.hpp"
#include "twrf/raster/math.hpp"
#include "twrf/raster/geometry.hpp"
#include "twrf/raster/texture.hpp"
#include "twrf/raster/scene.hpp"
#include "twrf/raster/tile_rasterizer.hpp"
#include "twrf/raster/framebuffer.hpp"
#include <iostream>
#include <memory>
#include <vector>

namespace twrf::raster {

constexpr ResourceId CAMERA_RESOURCE_ID = 1000;
constexpr ResourceId OBJECT_RESOURCE_BASE = 2000;
constexpr ResourceId TEXTURE_RESOURCE_BASE = 3000;
constexpr TWRId TILE_TWR_BASE = 10000;

struct RenderResult {
    uint64_t frame_index{0};
    uint64_t tiles_executed{0};
    uint64_t tiles_skipped{0};
    double skip_ratio{0.0};
    FrameBuffer framebuffer;
    uint64_t mutated_resources{0};
    std::vector<bool> executed_tiles;
};

class TWRFRenderer {
public:
    explicit TWRFRenderer(TileConfig config = TileConfig{})
        : config_(config),
          framebuffer_(config.frame_width, config.frame_height) {}

    [[nodiscard]] const TileConfig& config() const noexcept { return config_; }
    [[nodiscard]] Scene& scene() noexcept { return scene_; }
    [[nodiscard]] const Scene& scene() const noexcept { return scene_; }
    [[nodiscard]] const FrameBuffer& framebuffer() const noexcept { return framebuffer_; }
    [[nodiscard]] LogicalStateStore& state_store() noexcept { return state_store_; }
    [[nodiscard]] const ExecutionTrace& trace() const noexcept { return trace_; }
    [[nodiscard]] const MetricsCollector& metrics() const noexcept { return metrics_; }
    [[nodiscard]] const TWRGraph& graph() const noexcept { return *graph_; }
    [[nodiscard]] TWRGraph& graph() noexcept { return *graph_; }

    void initialize() {
        graph_ = std::make_unique<TWRGraph>();
        state_store_ = LogicalStateStore();
        scheduler_ = TWRScheduler();
        trace_.clear();
        metrics_.reset();
        current_frame_ = 0;
        mutated_resources_this_frame_ = 0;

        // 1. Register Camera resource
        auto& cam_res = graph_->add_resource(CAMERA_RESOURCE_ID, "Camera");
        self.observe_resource(CAMERA_RESOURCE_ID);
                for (const auto& obj : scene_.objects) self.observe_resource(OBJECT_RESOURCE_BASE + obj.id);
                for (size_t t = 0; t < scene_.textures.size(); ++t) self.observe_resource(TEXTURE_RESOURCE_BASE + static_cast<ResourceId>(t));

                Mat4 vp = scene_.camera.view_proj_matrix();
        cam_res.set_value(vp);

        // 2. Register Object transform resources
        scene_.update_all_screen_bounds(config_.frame_width, config_.frame_height);
        for (const auto& obj : scene_.objects) {
            ResourceId r_id = OBJECT_RESOURCE_BASE + obj.id;
            auto& obj_res = graph_->add_resource(r_id, "ObjTransform_" + std::to_string(obj.id), obj.screen_bounds);
            obj_res.set_value(obj.transform);
        }

        // 3. Register Texture resources
        for (size_t i = 0; i < scene_.textures.size(); ++i) {
            ResourceId t_id = TEXTURE_RESOURCE_BASE + static_cast<ResourceId>(i);
            auto& tex_res = graph_->add_resource(t_id, "Texture_" + std::to_string(i));
            tex_res.set_data(scene_.textures[i].pixels().data(), scene_.textures[i].size_bytes());
        }

        // 4. Create Tile TWRs
        int total_tiles = config_.total_tiles();
        for (int i = 0; i < total_tiles; ++i) {
            TWRId twr_id = TILE_TWR_BASE + i;
            BoundingRegion bounds = config_.get_tile_bounds(i);
            auto& twr = graph_->add_twr(twr_id, "Tile_" + std::to_string(i), bounds);

            // Bind camera
            graph_->bind_resource(twr.id(), CAMERA_RESOURCE_ID);

            // Bind all scene objects initially
            for (const auto& obj : scene_.objects) {
                graph_->bind_resource(twr.id(), OBJECT_RESOURCE_BASE + obj.id);
            }

            // Bind textures
            for (size_t t = 0; t < scene_.textures.size(); ++t) {
                graph_->bind_resource(twr.id(), TEXTURE_RESOURCE_BASE + static_cast<ResourceId>(t));
            }

            // Setup tile execution kernel
            twr.set_kernel([this, i, bounds](TemporalWorkRegion& self, LogicalStateStore& store,
                                             const std::vector<const VersionedResource*>&) -> bool {
                int tile_size = config_.tile_size;
                int pixel_count = tile_size * tile_size;

                std::vector<ColorRGBA> tile_color(pixel_count);
                std::vector<float> tile_depth(pixel_count);

                TileRasterizer::clear_tile(tile_color.data(), tile_depth.data(), tile_size,
                                           ColorRGBA{20, 20, 30, 255}, 1.0f);

                Mat4 vp = scene_.camera.view_proj_matrix();

                // Rasterize overlapping objects
                for (const auto& obj : scene_.objects) {
                    if (!bounds.overlaps(obj.screen_bounds)) {
                        continue; // Conservative culling
                    }

                    Mat4 mvp = vp * obj.transform;
                    const Texture* tex = (obj.texture_id < scene_.textures.size())
                                             ? &scene_.textures[obj.texture_id]
                                             : nullptr;

                    for (const auto& tri : obj.mesh.triangles()) {
                        TileRasterizer::rasterize_triangle_into_tile(
                            tri, mvp, tex,
                            config_.frame_width, config_.frame_height,
                            bounds.min_x, bounds.min_y, tile_size,
                            tile_color.data(), tile_depth.data()
                        );
                    }
                }

                // Payload size: colors (pixel_count * 4) + depths (pixel_count * 4)
                size_t payload_bytes = pixel_count * (sizeof(ColorRGBA) + sizeof(float));
                std::vector<uint8_t> payload(payload_bytes);
                std::memcpy(payload.data(), tile_color.data(), pixel_count * sizeof(ColorRGBA));
                std::memcpy(payload.data() + pixel_count * sizeof(ColorRGBA), tile_depth.data(), pixel_count * sizeof(float));

                if (!store.write_output(self.id(), payload.data(), payload.size(), self.current_output_version() + 1, 1)) return false;


                return true;
            });
        }

        graph_->validate_and_compute_depths();
    }

    void notify_camera_changed() {
        auto* cam_res = graph_->get_resource(CAMERA_RESOURCE_ID);
        if (cam_res) {
            Mat4 vp = scene_.camera.view_proj_matrix();
            cam_res->set_value(vp);
        }
        scene_.update_all_screen_bounds(config_.frame_width, config_.frame_height);

        // Camera move conservatively affects any tile overlapping visible objects
        mutated_resources_this_frame_ += scene_.objects.size();
        for (int i = 0; i < config_.total_tiles(); ++i) {
            TWRId tid = TILE_TWR_BASE + i;
            auto* twr = graph_->get_twr(tid);
            if (!twr) continue;
            for (const auto& obj : scene_.objects) {
                if (twr->region().overlaps(obj.screen_bounds) || twr->region().overlaps(obj.prev_screen_bounds)) {
                    twr->mark_dirty(ExecutionReason::ConservativeInvalidation);
                    break;
                }
            }
        }
    }

    void notify_object_transform_changed(uint32_t object_id) {
        auto* obj = scene_.get_object(object_id);
        if (!obj) return;

        mutated_resources_this_frame_++;

        Mat4 vp = scene_.camera.view_proj_matrix();
        obj->update_screen_bounds(vp, config_.frame_width, config_.frame_height);

        auto* res = graph_->get_resource(OBJECT_RESOURCE_BASE + object_id);
        if (res) {
            res->set_value(obj->transform);
            res->set_bounds(obj->screen_bounds);
        }

        // Conservative invalidation union: old bounds U new bounds
        BoundingRegion invalid_union{
            std::min(obj->screen_bounds.min_x, obj->prev_screen_bounds.min_x),
            std::min(obj->screen_bounds.min_y, obj->prev_screen_bounds.min_y),
            std::max(obj->screen_bounds.max_x, obj->prev_screen_bounds.max_x),
            std::max(obj->screen_bounds.max_y, obj->prev_screen_bounds.max_y)
        };

        for (int i = 0; i < config_.total_tiles(); ++i) {
            TWRId tid = TILE_TWR_BASE + i;
            auto* twr = graph_->get_twr(tid);
            if (twr && twr->region().overlaps(invalid_union)) {
                twr->mark_dirty(ExecutionReason::ConservativeInvalidation);
            }
        }
    }

    void notify_texture_changed(uint32_t texture_id) {
        if (texture_id >= scene_.textures.size()) return;
        mutated_resources_this_frame_++;
        auto* res = graph_->get_resource(TEXTURE_RESOURCE_BASE + texture_id);
        if (res) {
            res->set_data(scene_.textures[texture_id].pixels().data(), scene_.textures[texture_id].size_bytes());
        }

        // Invalidate all tiles containing objects that use this texture
        for (const auto& obj : scene_.objects) {
            if (obj.texture_id == texture_id) {
                for (int i = 0; i < config_.total_tiles(); ++i) {
                    TWRId tid = TILE_TWR_BASE + i;
                    auto* twr = graph_->get_twr(tid);
                    if (twr && twr->region().overlaps(obj.screen_bounds)) {
                        twr->mark_dirty(ExecutionReason::InputVersionChanged);
                    }
                }
            }
        }
    }

    RenderResult render_frame_incremental() {
        current_frame_++;
        uint64_t exec_before = metrics_.total_twr_executions;
        uint64_t skip_before = metrics_.total_twr_skips;
        size_t trace_start = trace_.entries().size();

        scheduler_.run_frame(*graph_, state_store_, trace_, metrics_);

        size_t trace_end = trace_.entries().size();
        uint64_t frame_execs = metrics_.total_twr_executions - exec_before;
        uint64_t frame_skips = metrics_.total_twr_skips - skip_before;
        double skip_ratio = (frame_execs + frame_skips > 0)
                                ? static_cast<double>(frame_skips) / (frame_execs + frame_skips)
                                : 0.0;

        std::vector<bool> executed_tiles(config_.total_tiles(), false);
        for (size_t idx = trace_start; idx < trace_end; ++idx) {
            const auto& e = trace_.entries()[idx];
            if (e.executed && e.twr_id >= TILE_TWR_BASE) {
                size_t tile_idx = e.twr_id - TILE_TWR_BASE;
                if (tile_idx < executed_tiles.size()) {
                    executed_tiles[tile_idx] = true;
                }
            }
        }

        // Compose final display FrameBuffer from all tile states
        assemble_framebuffer(framebuffer_);

        uint64_t frame_mutated = mutated_resources_this_frame_;
        mutated_resources_this_frame_ = 0;

        return RenderResult{
            current_frame_,
            frame_execs,
            frame_skips,
            skip_ratio,
            framebuffer_,
            frame_mutated,
            std::move(executed_tiles)
        };
    }

    // Forced-full-recompute reference oracle: evaluates every single tile from scratch
    FrameBuffer render_frame_forced_recompute() {
        FrameBuffer oracle_fb(config_.frame_width, config_.frame_height);
        int tile_size = config_.tile_size;
        int pixel_count = tile_size * tile_size;

        std::vector<ColorRGBA> tile_color(pixel_count);
        std::vector<float> tile_depth(pixel_count);

        Mat4 vp = scene_.camera.view_proj_matrix();

        for (int i = 0; i < config_.total_tiles(); ++i) {
            BoundingRegion bounds = config_.get_tile_bounds(i);
            TileRasterizer::clear_tile(tile_color.data(), tile_depth.data(), tile_size,
                                       ColorRGBA{20, 20, 30, 255}, 1.0f);

            for (const auto& obj : scene_.objects) {
                if (!bounds.overlaps(obj.screen_bounds)) continue;

                Mat4 mvp = vp * obj.transform;
                const Texture* tex = (obj.texture_id < scene_.textures.size())
                                         ? &scene_.textures[obj.texture_id]
                                         : nullptr;

                for (const auto& tri : obj.mesh.triangles()) {
                    TileRasterizer::rasterize_triangle_into_tile(
                        tri, mvp, tex,
                        config_.frame_width, config_.frame_height,
                        bounds.min_x, bounds.min_y, tile_size,
                        tile_color.data(), tile_depth.data()
                    );
                }
            }

            oracle_fb.blit_tile(bounds.min_x, bounds.min_y, tile_size, tile_size, tile_color.data());
        }

        return oracle_fb;
    }

private:
    void assemble_framebuffer(FrameBuffer& fb) {
        int tile_size = config_.tile_size;
        int pixel_count = tile_size * tile_size;

        for (int i = 0; i < config_.total_tiles(); ++i) {
            TWRId tid = TILE_TWR_BASE + i;
            BoundingRegion bounds = config_.get_tile_bounds(i);

            size_t sz = 0;
            VersionNumber ver = 0;
            const uint8_t* payload = state_store_.read_output(tid, sz, ver);

            if (payload && sz >= pixel_count * sizeof(ColorRGBA)) {
                const auto* colors = reinterpret_cast<const ColorRGBA*>(payload);
                fb.blit_tile(bounds.min_x, bounds.min_y, tile_size, tile_size, colors);
            }
        }
    }

    TileConfig config_;
    Scene scene_;
    FrameBuffer framebuffer_;

    std::unique_ptr<TWRGraph> graph_;
    LogicalStateStore state_store_;
    TWRScheduler scheduler_;
    ExecutionTrace trace_;
    MetricsCollector metrics_;

    uint64_t current_frame_{0};
    uint64_t mutated_resources_this_frame_{0};
};

} // namespace twrf::raster
