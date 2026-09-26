#pragma once

#include "twrf/core/types.hpp"
#include "twrf/core/state_store.hpp"
#include "twrf/raster/texture.hpp"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstring>
#include <optional>
#include <algorithm>

namespace twrf {

struct TWR3DTileSlot {
    VersionNumber color_version{INVALID_VERSION};
    VersionNumber depth_version{INVALID_VERSION};
    std::vector<raster::ColorRGBA> color_buffer;
    std::vector<float> depth_buffer;
    uint32_t tile_x{0};
    uint32_t tile_y{0};
    uint32_t tile_w{16};
    uint32_t tile_h{16};
    bool is_hud_layer{false};
    Timestamp last_updated_step{0};
};

struct StateStore3DMetrics {
    size_t total_color_bytes{0};
    size_t total_depth_bytes{0};
    uint64_t total_tile_reads{0};
    uint64_t total_tile_writes{0};
    uint64_t depth_occlusion_tests{0};
    uint64_t depth_occlusion_culls{0};
};

class LogicalStateStore3D {
public:
    explicit LogicalStateStore3D(size_t capacity_limit_bytes = 0)
        : capacity_limit_bytes_(capacity_limit_bytes) {}

    void set_capacity_limit(size_t limit_bytes) noexcept {
        capacity_limit_bytes_ = limit_bytes;
    }

    [[nodiscard]] const StateStore3DMetrics& metrics() const noexcept {
        return metrics_;
    }

    void reset_metrics() noexcept {
        metrics_ = StateStore3DMetrics{};
    }

    bool has_slot(TWRId id) const noexcept {
        return slots_.find(id) != slots_.end();
    }

    void allocate_tile(TWRId id, uint32_t tile_x, uint32_t tile_y, uint32_t tile_w = 16, uint32_t tile_h = 16, bool is_hud = false) {
        auto& slot = slots_[id];
        slot.tile_x = tile_x;
        slot.tile_y = tile_y;
        slot.tile_w = tile_w;
        slot.tile_h = tile_h;
        slot.is_hud_layer = is_hud;

        size_t count = tile_w * tile_h;
        slot.color_buffer.resize(count, raster::ColorRGBA::black());
        slot.depth_buffer.resize(count, 1.0f);

        metrics_.total_color_bytes += count * sizeof(raster::ColorRGBA);
        metrics_.total_depth_bytes += count * sizeof(float);
    }

    bool write_tile(TWRId id, const raster::ColorRGBA* colors, const float* depths, size_t count,
                    VersionNumber color_ver, VersionNumber depth_ver, Timestamp step = 0) {
        auto it = slots_.find(id);
        if (it == slots_.end()) return false;

        auto& slot = it->second;
        if (count != slot.color_buffer.size()) {
            slot.color_buffer.resize(count);
            slot.depth_buffer.resize(count);
        }

        if (colors) {
            std::memcpy(slot.color_buffer.data(), colors, count * sizeof(raster::ColorRGBA));
            slot.color_version = color_ver;
        }

        if (depths) {
            std::memcpy(slot.depth_buffer.data(), depths, count * sizeof(float));
            slot.depth_version = depth_ver;
        }

        slot.last_updated_step = step;
        metrics_.total_tile_writes++;
        return true;
    }

    [[nodiscard]] const TWR3DTileSlot* read_tile(TWRId id) const {
        auto it = slots_.find(id);
        if (it == slots_.end()) return nullptr;
        metrics_.total_tile_reads++;
        return &it->second;
    }

    // Depth buffer occlusion query: tests if a 3D bounding box / depth is entirely occluded by cached background
    [[nodiscard]] bool is_depth_occluded(TWRId id, float min_primitive_depth) const noexcept {
        auto it = slots_.find(id);
        if (it == slots_.end()) return false;

        metrics_.depth_occlusion_tests++;
        const auto& depths = it->second.depth_buffer;

        // If all cached depth pixels are strictly closer than min_primitive_depth, primitive is completely hidden
        for (float d : depths) {
            if (min_primitive_depth <= d) {
                return false; // Visible at least partially
            }
        }

        metrics_.depth_occlusion_culls++;
        return true; // Completely occluded!
    }

    [[nodiscard]] size_t total_memory_bytes() const noexcept {
        return metrics_.total_color_bytes + metrics_.total_depth_bytes;
    }

    [[nodiscard]] size_t slot_count() const noexcept {
        return slots_.size();
    }

private:
    size_t capacity_limit_bytes_{0};
    mutable StateStore3DMetrics metrics_;
    std::unordered_map<TWRId, TWR3DTileSlot> slots_;
};

} // namespace twrf
