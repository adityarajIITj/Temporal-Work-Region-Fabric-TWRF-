#pragma once

#include "twrf/raster/math.hpp"
#include <vector>
#include <array>
#include <limits>
#include <algorithm>

namespace twrf::raster {

struct Vertex {
    Vec3 pos;
    Vec3 normal{0, 0, 1};
    Vec2 uv{0, 0};
    Vec4 color{1, 1, 1, 1};

    constexpr Vertex() = default;
    constexpr Vertex(const Vec3& p, const Vec2& uv_ = {0, 0}, const Vec4& c = {1, 1, 1, 1})
        : pos(p), uv(uv_), color(c) {}
};

struct Triangle {
    std::array<Vertex, 3> v;
    uint32_t texture_id{0};

    constexpr Triangle() = default;
    constexpr Triangle(const Vertex& v0, const Vertex& v1, const Vertex& v2, uint32_t tex = 0)
        : v{v0, v1, v2}, texture_id(tex) {}
};

class Mesh {
public:
    Mesh() = default;
    explicit Mesh(std::vector<Triangle> tris) : triangles_(std::move(tris)) {}

    [[nodiscard]] const std::vector<Triangle>& triangles() const noexcept {
        return triangles_;
    }

    void add_triangle(const Triangle& tri) {
        triangles_.push_back(tri);
    }

    // Projects 3D mesh vertices through MVP to compute conservative 2D screen-space bounding box
    [[nodiscard]] BoundingRegion compute_screen_bounds(const Mat4& mvp, int screen_w, int screen_h) const noexcept {
        float min_x = std::numeric_limits<float>::max();
        float min_y = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float max_y = std::numeric_limits<float>::lowest();

        bool has_visible = false;
        for (const auto& tri : triangles_) {
            for (const auto& vert : tri.v) {
                Vec4 clip = mvp * Vec4(vert.pos, 1.0f);
                if (clip.w <= 0.001f) continue; // Behind near plane

                float ndc_x = clip.x / clip.w;
                float ndc_y = clip.y / clip.w;

                // Screen coordinates
                float sx = (ndc_x + 1.0f) * 0.5f * screen_w;
                float sy = (1.0f - (ndc_y + 1.0f) * 0.5f) * screen_h;

                min_x = std::min(min_x, sx);
                min_y = std::min(min_y, sy);
                max_x = std::max(max_x, sx);
                max_y = std::max(max_y, sy);
                has_visible = true;
            }
        }

        if (!has_visible) {
            return BoundingRegion{0, 0, 0, 0}; // Empty
        }

        int32_t x0 = std::clamp(static_cast<int32_t>(std::floor(min_x)), 0, screen_w);
        int32_t y0 = std::clamp(static_cast<int32_t>(std::floor(min_y)), 0, screen_h);
        int32_t x1 = std::clamp(static_cast<int32_t>(std::ceil(max_x)), 0, screen_w);
        int32_t y1 = std::clamp(static_cast<int32_t>(std::ceil(max_y)), 0, screen_h);

        return BoundingRegion::create(x0, y0, x1, y1);
    }

    static Mesh create_triangle(float size = 1.0f, uint32_t tex_id = 0) {
        Mesh m;
        Vertex v0(Vec3(-size, -size, 0.0f), Vec2(0.0f, 0.0f), Vec4(1, 0, 0, 1));
        Vertex v1(Vec3( size, -size, 0.0f), Vec2(1.0f, 0.0f), Vec4(0, 1, 0, 1));
        Vertex v2(Vec3( 0.0f,  size, 0.0f), Vec2(0.5f, 1.0f), Vec4(0, 0, 1, 1));
        m.add_triangle(Triangle(v0, v1, v2, tex_id));
        return m;
    }

    static Mesh create_quad(float w = 1.0f, float h = 1.0f, uint32_t tex_id = 0) {
        Mesh m;
        Vertex v0(Vec3(-w, -h, 0.0f), Vec2(0.0f, 0.0f), Vec4(1, 1, 1, 1));
        Vertex v1(Vec3( w, -h, 0.0f), Vec2(1.0f, 0.0f), Vec4(1, 1, 1, 1));
        Vertex v2(Vec3( w,  h, 0.0f), Vec2(1.0f, 1.0f), Vec4(1, 1, 1, 1));
        Vertex v3(Vec3(-w,  h, 0.0f), Vec2(0.0f, 1.0f), Vec4(1, 1, 1, 1));

        m.add_triangle(Triangle(v0, v1, v2, tex_id));
        m.add_triangle(Triangle(v0, v2, v3, tex_id));
        return m;
    }

    static Mesh create_cube(float s = 1.0f, uint32_t tex_id = 0) {
        Mesh m;
        // 6 faces * 2 triangles = 12 triangles
        auto add_face = [&](const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec4& col) {
            Vertex v0(p0, Vec2(0, 0), col);
            Vertex v1(p1, Vec2(1, 0), col);
            Vertex v2(p2, Vec2(1, 1), col);
            Vertex v3(p3, Vec2(0, 1), col);
            m.add_triangle(Triangle(v0, v1, v2, tex_id));
            m.add_triangle(Triangle(v0, v2, v3, tex_id));
        };

        // Front, Back, Left, Right, Top, Bottom
        add_face(Vec3(-s, -s,  s), Vec3( s, -s,  s), Vec3( s,  s,  s), Vec3(-s,  s,  s), Vec4(1, 0, 0, 1));
        add_face(Vec3( s, -s, -s), Vec3(-s, -s, -s), Vec3(-s,  s, -s), Vec3( s,  s, -s), Vec4(0, 1, 0, 1));
        add_face(Vec3(-s, -s, -s), Vec3(-s, -s,  s), Vec3(-s,  s,  s), Vec3(-s,  s, -s), Vec4(0, 0, 1, 1));
        add_face(Vec3( s, -s,  s), Vec3( s, -s, -s), Vec3( s,  s, -s), Vec3( s,  s,  s), Vec4(1, 1, 0, 1));
        add_face(Vec3(-s,  s,  s), Vec3( s,  s,  s), Vec3( s,  s, -s), Vec3(-s,  s, -s), Vec4(0, 1, 1, 1));
        add_face(Vec3(-s, -s, -s), Vec3( s, -s, -s), Vec3( s, -s,  s), Vec3(-s, -s,  s), Vec4(1, 0, 1, 1));

        return m;
    }

private:
    std::vector<Triangle> triangles_;
};

} // namespace twrf::raster
