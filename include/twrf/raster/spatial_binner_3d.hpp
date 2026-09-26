#pragma once

#include "twrf/core/types.hpp"
#include "twrf/raster/math.hpp"
#include "twrf/raster/tile_rasterizer.hpp"
#include <vector>
#include <array>
#include <string>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <optional>

namespace twrf::raster {

struct AABB3D {
    Vec3 min_pt{-1.0f, -1.0f, -1.0f};
    Vec3 max_pt{1.0f, 1.0f, 1.0f};

    constexpr AABB3D() = default;
    constexpr AABB3D(const Vec3& minp, const Vec3& maxp) : min_pt(minp), max_pt(maxp) {}

    [[nodiscard]] std::array<Vec3, 8> get_corners() const noexcept {
        return {
            Vec3{min_pt.x, min_pt.y, min_pt.z},
            Vec3{max_pt.x, min_pt.y, min_pt.z},
            Vec3{min_pt.x, max_pt.y, min_pt.z},
            Vec3{max_pt.x, max_pt.y, min_pt.z},
            Vec3{min_pt.x, min_pt.y, max_pt.z},
            Vec3{max_pt.x, min_pt.y, max_pt.z},
            Vec3{min_pt.x, max_pt.y, max_pt.z},
            Vec3{max_pt.x, max_pt.y, max_pt.z}
        };
    }

    [[nodiscard]] AABB3D transformed(const Mat4& mat) const noexcept {
        auto corners = get_corners();
        Vec3 new_min{1e30f, 1e30f, 1e30f};
        Vec3 new_max{-1e30f, -1e30f, -1e30f};

        for (const auto& c : corners) {
            Vec4 tc = mat * Vec4(c, 1.0f);
            new_min.x = std::min(new_min.x, tc.x);
            new_min.y = std::min(new_min.y, tc.y);
            new_min.z = std::min(new_min.z, tc.z);
            new_max.x = std::max(new_max.x, tc.x);
            new_max.y = std::max(new_max.y, tc.y);
            new_max.z = std::max(new_max.z, tc.z);
        }
        return AABB3D{new_min, new_max};
    }
};

// Projects a 3D AABB to a 2D Screen Bounding Region
inline std::optional<BoundingRegion> project_aabb_to_screen(
    const AABB3D& aabb, const Mat4& model_to_clip, int screen_w, int screen_h) noexcept {

    auto corners = aabb.get_corners();
    float min_x = 1e30f, max_x = -1e30f;
    float min_y = 1e30f, max_y = -1e30f;
    int visible_corners = 0;

    for (const auto& c : corners) {
        Vec4 clip = model_to_clip * Vec4(c, 1.0f);
        if (clip.w <= 0.01f) continue; // Behind near plane

        float inv_w = 1.0f / clip.w;
        float ndc_x = clip.x * inv_w;
        float ndc_y = clip.y * inv_w;

        // Map NDC [-1, 1] to screen [0, W], [0, H]
        float sx = (ndc_x * 0.5f + 0.5f) * screen_w;
        float sy = (1.0f - (ndc_y * 0.5f + 0.5f)) * screen_h;

        min_x = std::min(min_x, sx);
        max_x = std::max(max_x, sx);
        min_y = std::min(min_y, sy);
        max_y = std::max(max_y, sy);
        ++visible_corners;
    }

    if (visible_corners == 0) return std::nullopt; // Entirely behind camera

    int32_t x0 = std::max<int32_t>(0, static_cast<int32_t>(std::floor(min_x)));
    int32_t y0 = std::max<int32_t>(0, static_cast<int32_t>(std::floor(min_y)));
    int32_t x1 = std::min<int32_t>(screen_w, static_cast<int32_t>(std::ceil(max_x)));
    int32_t y1 = std::min<int32_t>(screen_h, static_cast<int32_t>(std::ceil(max_y)));

    if (x0 >= x1 || y0 >= y1) return std::nullopt;

    return BoundingRegion::create(x0, y0, x1, y1);
}

struct DynamicEntity3D {
    uint32_t id{0};
    std::string name;
    AABB3D local_box;
    Mat4 world_transform = Mat4::identity();
    VersionNumber version{1};
    bool has_moved{false};
};

class SpatialBinner3D {
public:
    explicit SpatialBinner3D(TileConfig config) : config_(config) {
        tile_dirty_.resize(config_.total_tiles(), false);
    }

    void register_entity(uint32_t id, const std::string& name, const AABB3D& box, const Mat4& initial_transform) {
        DynamicEntity3D e;
        e.id = id;
        e.name = name;
        e.local_box = box;
        e.world_transform = initial_transform;
        e.version = 1;
        e.has_moved = true; // Cold frame marks moved
        entities_[id] = e;
    }

    void update_entity_transform(uint32_t id, const Mat4& new_transform) {
        auto it = entities_.find(id);
        if (it == entities_.end()) return;

        // Check if transform actually changed
        bool changed = false;
        for (size_t i = 0; i < 16; ++i) {
            if (std::abs(it->second.world_transform.m[i] - new_transform.m[i]) > 1e-4f) {
                changed = true;
                break;
            }
        }

        if (changed) {
            it->second.world_transform = new_transform;
            it->second.version++;
            it->second.has_moved = true;
        }
    }

    // Evaluate spatial binning and compute dirty tiles for the frame
    void evaluate_frame(const Mat4& view_proj, bool camera_moved = false) {
        std::fill(tile_dirty_.begin(), tile_dirty_.end(), false);

        if (camera_moved) {
            // Camera motion invalidates all 3D world tiles (HUD tiles handled separately)
            std::fill(tile_dirty_.begin(), tile_dirty_.end(), true);
            for (auto& [id, e] : entities_) {
                e.has_moved = false;
            }
            return;
        }

        // Camera is static: only tiles intersected by moving dynamic entities are dirty!
        int tiles_x = config_.tiles_x();
        int tile_size = config_.tile_size;

        for (auto& [id, e] : entities_) {
            if (!e.has_moved) continue;

            Mat4 mvp = view_proj * e.world_transform;
            auto screen_bounds = project_aabb_to_screen(
                e.local_box, mvp, config_.frame_width, config_.frame_height);

            if (!screen_bounds.has_value()) {
                e.has_moved = false;
                continue;
            }

            // Invalidate all intersecting tiles using min_x, max_x, min_y, max_y
            int min_tx = screen_bounds->min_x / tile_size;
            int max_tx = std::min(config_.tiles_x() - 1, (screen_bounds->max_x - 1) / tile_size);
            int min_ty = screen_bounds->min_y / tile_size;
            int max_ty = std::min(config_.tiles_y() - 1, (screen_bounds->max_y - 1) / tile_size);

            for (int ty = min_ty; ty <= max_ty; ++ty) {
                for (int tx = min_tx; tx <= max_tx; ++tx) {
                    int t_idx = ty * tiles_x + tx;
                    if (t_idx >= 0 && t_idx < config_.total_tiles()) {
                        tile_dirty_[t_idx] = true;
                    }
                }
            }

            e.has_moved = false;
        }
    }

    [[nodiscard]] bool is_tile_dirty(int tile_idx) const noexcept {
        if (tile_idx < 0 || tile_idx >= config_.total_tiles()) return true;
        return tile_dirty_[tile_idx];
    }

    [[nodiscard]] uint32_t dirty_tile_count() const noexcept {
        uint32_t count = 0;
        for (bool d : tile_dirty_) {
            if (d) ++count;
        }
        return count;
    }

    [[nodiscard]] uint32_t clean_tile_count() const noexcept {
        return static_cast<uint32_t>(config_.total_tiles()) - dirty_tile_count();
    }

    [[nodiscard]] double skip_ratio() const noexcept {
        return static_cast<double>(clean_tile_count()) / config_.total_tiles();
    }

    [[nodiscard]] const TileConfig& config() const noexcept {
        return config_;
    }

private:
    TileConfig config_;
    std::unordered_map<uint32_t, DynamicEntity3D> entities_;
    std::vector<bool> tile_dirty_;
};

} // namespace twrf::raster
