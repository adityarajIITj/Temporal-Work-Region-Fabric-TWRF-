#pragma once

#include "twrf/core/types.hpp"
#include <cmath>
#include <array>
#include <algorithm>

namespace twrf::raster {

constexpr float PI = 3.14159265358979323846f;

struct Vec2 {
    float x{0.0f}, y{0.0f};

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& o) const noexcept { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const noexcept { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const noexcept { return {x * s, y * s}; }
};

struct Vec3 {
    float x{0.0f}, y{0.0f}, z{0.0f};

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }

    [[nodiscard]] float dot(const Vec3& o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }

    [[nodiscard]] Vec3 cross(const Vec3& o) const noexcept {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(dot(*this));
    }

    [[nodiscard]] Vec3 normalized() const noexcept {
        float len = length();
        return len > 1e-6f ? (*this) * (1.0f / len) : Vec3{};
    }
};

struct Vec4 {
    float x{0.0f}, y{0.0f}, z{0.0f}, w{1.0f};

    constexpr Vec4() = default;
    constexpr Vec4(float x_, float y_, float z_, float w_ = 1.0f) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Vec4(const Vec3& v, float w_ = 1.0f) : x(v.x), y(v.y), z(v.z), w(w_) {}

    Vec4 operator+(const Vec4& o) const noexcept { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    Vec4 operator*(float s) const noexcept { return {x * s, y * s, z * s, w * s}; }
};

// 4x4 matrix, column-major order
struct Mat4 {
    std::array<float, 16> m{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    static Mat4 identity() noexcept {
        return Mat4{};
    }

    static Mat4 translation(float tx, float ty, float tz) noexcept {
        Mat4 res = identity();
        res.m[12] = tx;
        res.m[13] = ty;
        res.m[14] = tz;
        return res;
    }

    static Mat4 scale(float sx, float sy, float sz) noexcept {
        Mat4 res = identity();
        res.m[0] = sx;
        res.m[5] = sy;
        res.m[10] = sz;
        return res;
    }

    static Mat4 rotation_y(float rad) noexcept {
        Mat4 res = identity();
        float c = std::cos(rad);
        float s = std::sin(rad);
        res.m[0] = c;
        res.m[2] = -s;
        res.m[8] = s;
        res.m[10] = c;
        return res;
    }

    static Mat4 rotation_x(float rad) noexcept {
        Mat4 res = identity();
        float c = std::cos(rad);
        float s = std::sin(rad);
        res.m[5] = c;
        res.m[6] = s;
        res.m[9] = -s;
        res.m[10] = c;
        return res;
    }

    static Mat4 perspective(float fov_rad, float aspect, float near_z, float far_z) noexcept {
        Mat4 res{};
        float tan_half_fov = std::tan(fov_rad / 2.0f);
        res.m[0] = 1.0f / (aspect * tan_half_fov);
        res.m[5] = 1.0f / tan_half_fov;
        res.m[10] = -(far_z + near_z) / (far_z - near_z);
        res.m[11] = -1.0f;
        res.m[14] = -(2.0f * far_z * near_z) / (far_z - near_z);
        return res;
    }

    static Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up) noexcept {
        Vec3 f = (target - eye).normalized();
        Vec3 s = f.cross(up).normalized();
        Vec3 u = s.cross(f);

        Mat4 res = identity();
        res.m[0] = s.x;
        res.m[4] = s.y;
        res.m[8] = s.z;

        res.m[1] = u.x;
        res.m[5] = u.y;
        res.m[9] = u.z;

        res.m[2] = -f.x;
        res.m[6] = -f.y;
        res.m[10] = -f.z;

        res.m[12] = -s.dot(eye);
        res.m[13] = -u.dot(eye);
        res.m[14] = f.dot(eye);
        return res;
    }

    Mat4 operator*(const Mat4& o) const noexcept {
        Mat4 res{};
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += m[k * 4 + r] * o.m[c * 4 + k];
                }
                res.m[c * 4 + r] = sum;
            }
        }
        return res;
    }

    Vec4 operator*(const Vec4& v) const noexcept {
        return {
            m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
            m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
            m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w
        };
    }
};

// Computes 2D edge function for barycentric rasterization
inline float edge_function(const Vec2& a, const Vec2& b, const Vec2& c) noexcept {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

} // namespace twrf::raster
