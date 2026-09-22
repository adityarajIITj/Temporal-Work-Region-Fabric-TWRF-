#pragma once

#include "twrf/core/types.hpp"
#include "twrf/raster/math.hpp"
#include "twrf/raster/geometry.hpp"
#include "twrf/raster/texture.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

namespace twrf::raster {

struct TileConfig {
    int frame_width{256};
    int frame_height{256};
    int tile_size{16};

    [[nodiscard]] int tiles_x() const noexcept {
        return (frame_width + tile_size - 1) / tile_size;
    }

    [[nodiscard]] int tiles_y() const noexcept {
        return (frame_height + tile_size - 1) / tile_size;
    }

    [[nodiscard]] int total_tiles() const noexcept {
        return tiles_x() * tiles_y();
    }

    [[nodiscard]] BoundingRegion get_tile_bounds(int tile_idx) const noexcept {
        int tx = tile_idx % tiles_x();
        int ty = tile_idx / tiles_x();
        int x0 = tx * tile_size;
        int y0 = ty * tile_size;
        int x1 = std::min(x0 + tile_size, frame_width);
        int y1 = std::min(y0 + tile_size, frame_height);
        return BoundingRegion::create(x0, y0, x1, y1);
    }
};

class TileRasterizer {
public:
    static void clear_tile(ColorRGBA* color_buf, float* depth_buf, int tile_size,
                           ColorRGBA clear_col = ColorRGBA::black(), float clear_depth = 1.0f) noexcept {
        int count = tile_size * tile_size;
        for (int i = 0; i < count; ++i) {
            color_buf[i] = clear_col;
            depth_buf[i] = clear_depth;
        }
    }

    static void rasterize_triangle_into_tile(const Triangle& tri, const Mat4& mvp,
                                            const Texture* tex,
                                            int screen_w, int screen_h,
                                            int tile_x0, int tile_y0, int tile_size,
                                            ColorRGBA* color_buf, float* depth_buf) noexcept {
        // 1. Vertex transformation to clip space
        Vec4 c0 = mvp * Vec4(tri.v[0].pos, 1.0f);
        Vec4 c1 = mvp * Vec4(tri.v[1].pos, 1.0f);
        Vec4 c2 = mvp * Vec4(tri.v[2].pos, 1.0f);

        // Near-plane culling
        if (c0.w <= 0.001f && c1.w <= 0.001f && c2.w <= 0.001f) return;
        if (c0.w <= 0.001f || c1.w <= 0.001f || c2.w <= 0.001f) return;

        // 2. Perspective divide -> Screen space coordinates
        auto to_screen = [&](const Vec4& c) -> Vec3 {
            float ndc_x = c.x / c.w;
            float ndc_y = c.y / c.w;
            float ndc_z = (c.z / c.w + 1.0f) * 0.5f; // [0, 1] range
            float sx = (ndc_x + 1.0f) * 0.5f * screen_w;
            float sy = (1.0f - (ndc_y + 1.0f) * 0.5f) * screen_h;
            return {sx, sy, ndc_z};
        };

        Vec3 p0 = to_screen(c0);
        Vec3 p1 = to_screen(c1);
        Vec3 p2 = to_screen(c2);

        // Triangle screen bounds
        float tri_min_x = std::min({p0.x, p1.x, p2.x});
        float tri_max_x = std::max({p0.x, p1.x, p2.x});
        float tri_min_y = std::min({p0.y, p1.y, p2.y});
        float tri_max_y = std::max({p0.y, p1.y, p2.y});

        int tile_x1 = tile_x0 + tile_size;
        int tile_y1 = tile_y0 + tile_size;

        // Check tile overlap
        if (tri_max_x < tile_x0 || tri_min_x >= tile_x1 ||
            tri_max_y < tile_y0 || tri_min_y >= tile_y1) {
            return;
        }

        // Clamp scan range to tile bounds
        int start_x = std::max(tile_x0, static_cast<int>(std::floor(tri_min_x)));
        int end_x   = std::min(tile_x1 - 1, static_cast<int>(std::ceil(tri_max_x)));
        int start_y = std::max(tile_y0, static_cast<int>(std::floor(tri_min_y)));
        int end_y   = std::min(tile_y1 - 1, static_cast<int>(std::ceil(tri_max_y)));

        Vec2 v0{p0.x, p0.y};
        Vec2 v1{p1.x, p1.y};
        Vec2 v2{p2.x, p2.y};

        float area = edge_function(v0, v1, v2);
        if (std::abs(area) < 1e-6f) return; // Degenerate

        float inv_area = 1.0f / area;

        for (int y = start_y; y <= end_y; ++y) {
            for (int x = start_x; x <= end_x; ++x) {
                Vec2 p{x + 0.5f, y + 0.5f};

                float w0 = edge_function(v1, v2, p) * inv_area;
                float w1 = edge_function(v2, v0, p) * inv_area;
                float w2 = edge_function(v0, v1, p) * inv_area;

                // Support both clockwise and counter-clockwise rendering
                bool inside = (area > 0) ? (w0 >= -1e-4f && w1 >= -1e-4f && w2 >= -1e-4f)
                                         : (w0 <= 1e-4f && w1 <= 1e-4f && w2 <= 1e-4f);

                if (inside) {
                    float z = w0 * p0.z + w1 * p1.z + w2 * p2.z;
                    int local_x = x - tile_x0;
                    int local_y = y - tile_y0;
                    int idx = local_y * tile_size + local_x;

                    if (z >= 0.0f && z < depth_buf[idx]) {
                        depth_buf[idx] = z;

                        // Interpolate UV and color
                        float u = w0 * tri.v[0].uv.x + w1 * tri.v[1].uv.x + w2 * tri.v[2].uv.x;
                        float v = w0 * tri.v[0].uv.y + w1 * tri.v[1].uv.y + w2 * tri.v[2].uv.y;

                        ColorRGBA final_color;
                        if (tex) {
                            final_color = tex->sample(u, v);
                        } else {
                            float r = w0 * tri.v[0].color.x + w1 * tri.v[1].color.x + w2 * tri.v[2].color.x;
                            float g = w0 * tri.v[0].color.y + w1 * tri.v[1].color.y + w2 * tri.v[2].color.y;
                            float b = w0 * tri.v[0].color.z + w1 * tri.v[1].color.z + w2 * tri.v[2].color.z;
                            final_color = ColorRGBA(
                                static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f)),
                                static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f)),
                                static_cast<uint8_t>(std::clamp(b * 255.0f, 0.0f, 255.0f)),
                                255
                            );
                        }
                        color_buf[idx] = final_color;
                    }
                }
            }
        }
    }
};

} // namespace twrf::raster
