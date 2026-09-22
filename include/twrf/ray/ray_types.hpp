#pragma once

#include "twrf/core/types.hpp"
#include "twrf/raster/math.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>

namespace twrf::ray {

using raster::Vec3;
using raster::Vec2;
using raster::Mat4;

struct Ray {
    Vec3 origin;
    Vec3 direction; // Normalized
    float t_min{0.001f};
    float t_max{std::numeric_limits<float>::infinity()};

    constexpr Ray() = default;
    constexpr Ray(const Vec3& orig, const Vec3& dir, float tmin = 0.001f, float tmax = std::numeric_limits<float>::infinity())
        : origin(orig), direction(dir), t_min(tmin), t_max(tmax) {}

    [[nodiscard]] Vec3 point_at(float t) const noexcept {
        return origin + direction * t;
    }
};

struct RayHit {
    bool hit{false};
    float t{std::numeric_limits<float>::infinity()};
    Vec3 point;
    Vec3 normal;
    uint32_t object_id{0};
    uint32_t material_id{0};
};

struct Sphere {
    uint32_t id{0};
    Vec3 center;
    float radius{1.0f};
    uint32_t material_id{0};

    bool intersect(const Ray& ray, RayHit& out_hit) const noexcept {
        Vec3 oc = ray.origin - center;
        float a = ray.direction.dot(ray.direction);
        float b = 2.0f * oc.dot(ray.direction);
        float c = oc.dot(oc) - radius * radius;
        float discriminant = b * b - 4.0f * a * c;

        if (discriminant < 0.0f) {
            return false;
        }

        float sqrt_d = std::sqrt(discriminant);
        float t = (-b - sqrt_d) / (2.0f * a);
        if (t < ray.t_min || t > ray.t_max) {
            t = (-b + sqrt_d) / (2.0f * a);
            if (t < ray.t_min || t > ray.t_max) {
                return false;
            }
        }

        if (t < out_hit.t) {
            out_hit.hit = true;
            out_hit.t = t;
            out_hit.point = ray.point_at(t);
            out_hit.normal = (out_hit.point - center).normalized();
            out_hit.object_id = id;
            out_hit.material_id = material_id;
            return true;
        }
        return false;
    }
};

struct RayAABB {
    Vec3 min_pt{-1.0f, -1.0f, -1.0f};
    Vec3 max_pt{ 1.0f,  1.0f,  1.0f};

    [[nodiscard]] bool intersect(const Ray& ray) const noexcept {
        float t0 = ray.t_min;
        float t1 = ray.t_max;

        for (int a = 0; a < 3; ++a) {
            float origin_val = (a == 0) ? ray.origin.x : ((a == 1) ? ray.origin.y : ray.origin.z);
            float dir_val = (a == 0) ? ray.direction.x : ((a == 1) ? ray.direction.y : ray.direction.z);
            float min_val = (a == 0) ? min_pt.x : ((a == 1) ? min_pt.y : min_pt.z);
            float max_val = (a == 0) ? max_pt.x : ((a == 1) ? max_pt.y : max_pt.z);

            if (std::abs(dir_val) < 1e-8f) {
                if (origin_val < min_val || origin_val > max_val) return false;
            } else {
                float inv_d = 1.0f / dir_val;
                float t_near = (min_val - origin_val) * inv_d;
                float t_far = (max_val - origin_val) * inv_d;
                if (t_near > t_far) std::swap(t_near, t_far);
                t0 = std::max(t0, t_near);
                t1 = std::min(t1, t_far);
                if (t1 < t0) return false;
            }
        }
        return true;
    }
};

struct RayBatch {
    uint32_t batch_id{0};
    BoundingRegion screen_region; // Spatial region if associated with a screen tile
    std::vector<Ray> rays;

    [[nodiscard]] size_t size() const noexcept { return rays.size(); }
};

struct RayBatchPayload {
    uint32_t batch_id{0};
    uint32_t ray_count{0};
    // Packed hit records: hit (1/0), t, normal (nx, ny, nz), occlusion fraction
    std::vector<float> hit_distances;
    std::vector<Vec3> hit_normals;
    std::vector<float> occlusions; // 0.0 = unoccluded, 1.0 = fully shadowed
};

} // namespace twrf::ray
