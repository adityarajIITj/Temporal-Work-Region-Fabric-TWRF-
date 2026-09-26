#pragma once

#include "twrf/core/types.hpp"
#include "twrf/raster/math.hpp"
#include "twrf/raster/texture.hpp"
#include "twrf/raster/tile_rasterizer.hpp"
#include "twrf/core/state_store_3d.hpp"
#include <vector>
#include <cstdint>
#include <algorithm>

namespace twrf::raster {

enum class LayerType {
    World3D,
    OrthographicHUD
};

struct HUDElement {
    std::string name;
    BoundingRegion bounds;
    VersionNumber version{1};
    bool is_dirty{false};
};

class DualLayerCompositor {
public:
    explicit DualLayerCompositor(TileConfig config) : config_(config) {
        hud_tiles_.resize(config_.total_tiles(), false);
    }

    void register_hud_element(const std::string& name, int x0, int y0, int x1, int y1) {
        HUDElement elem;
        elem.name = name;
        elem.bounds = BoundingRegion::create(x0, y0, x1, y1);
        elem.version = 1;
        elem.is_dirty = true;
        elements_.push_back(elem);

        // Mark tiles belonging to HUD
        int tile_size = config_.tile_size;
        int min_tx = x0 / tile_size;
        int max_tx = std::min(config_.tiles_x() - 1, (x1 - 1) / tile_size);
        int min_ty = y0 / tile_size;
        int max_ty = std::min(config_.tiles_y() - 1, (y1 - 1) / tile_size);

        for (int ty = min_ty; ty <= max_ty; ++ty) {
            for (int tx = min_tx; tx <= max_tx; ++tx) {
                int t_idx = ty * config_.tiles_x() + tx;
                if (t_idx >= 0 && t_idx < config_.total_tiles()) {
                    hud_tiles_[t_idx] = true;
                }
            }
        }
    }

    void mark_hud_element_dirty(const std::string& name) {
        for (auto& elem : elements_) {
            if (elem.name == name) {
                elem.version++;
                elem.is_dirty = true;
            }
        }
    }

    [[nodiscard]] bool is_hud_tile(int tile_idx) const noexcept {
        if (tile_idx < 0 || tile_idx >= config_.total_tiles()) return false;
        return hud_tiles_[tile_idx];
    }

    // Evaluates whether a HUD tile requires redraw
    [[nodiscard]] bool is_hud_tile_dirty(int tile_idx) const noexcept {
        if (!is_hud_tile(tile_idx)) return false;

        auto tile_bounds = config_.get_tile_bounds(tile_idx);
        for (const auto& elem : elements_) {
            if (elem.is_dirty && elem.bounds.overlaps(tile_bounds)) {
                return true;
            }
        }
        return false;
    }

    void finish_frame() {
        for (auto& elem : elements_) {
            elem.is_dirty = false;
        }
    }

    [[nodiscard]] size_t total_hud_tiles() const noexcept {
        size_t count = 0;
        for (bool b : hud_tiles_) if (b) ++count;
        return count;
    }

private:
    TileConfig config_;
    std::vector<bool> hud_tiles_;
    std::vector<HUDElement> elements_;
};

} // namespace twrf::raster
