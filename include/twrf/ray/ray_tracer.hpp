#pragma once

#include "twrf/ray/ray_types.hpp"
#include "twrf/core/twr.hpp"
#include "twrf/core/graph.hpp"
#include "twrf/core/state_store.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/trace.hpp"
#include "twrf/core/metrics.hpp"
#include <vector>
#include <memory>
#include <iostream>

namespace twrf::ray {

constexpr ResourceId RAY_CAMERA_RESOURCE_ID = 5000;
constexpr ResourceId RAY_GEOMETRY_RESOURCE_ID = 5001;
constexpr ResourceId RAY_LIGHT_RESOURCE_ID = 5002;
constexpr TWRId RAY_BATCH_TWR_BASE = 20000;

struct RayLight {
    Vec3 position{0.0f, 10.0f, 0.0f};
    float intensity{1.0f};
};

struct RayScene {
    std::vector<Sphere> spheres;
    RayLight light;
    Vec3 camera_pos{0.0f, 0.0f, 5.0f};

    [[nodiscard]] RayHit trace_ray(const Ray& ray) const noexcept {
        RayHit closest;
        for (const auto& sp : spheres) {
            sp.intersect(ray, closest);
        }
        return closest;
    }

    [[nodiscard]] bool is_occluded(const Ray& shadow_ray) const noexcept {
        RayHit hit;
        for (const auto& sp : spheres) {
            if (sp.intersect(shadow_ray, hit)) {
                return true;
            }
        }
        return false;
    }
};

class RayBatchPipeline {
public:
    explicit RayBatchPipeline(size_t batch_count = 16, size_t rays_per_batch = 64)
        : batch_count_(batch_count), rays_per_batch_(rays_per_batch) {
        graph_ = std::make_unique<TWRGraph>();
    }

    [[nodiscard]] RayScene& scene() noexcept { return scene_; }
    [[nodiscard]] const RayScene& scene() const noexcept { return scene_; }
    [[nodiscard]] TWRGraph& graph() noexcept { return *graph_; }
    [[nodiscard]] const TWRGraph& graph() const noexcept { return *graph_; }
    [[nodiscard]] LogicalStateStore& state_store() noexcept { return state_store_; }
    [[nodiscard]] const ExecutionTrace& trace() const noexcept { return trace_; }
    [[nodiscard]] const MetricsCollector& metrics() const noexcept { return metrics_; }

    void initialize_default_batches() {
        batches_.resize(batch_count_);
        for (size_t b = 0; b < batch_count_; ++b) {
            batches_[b].batch_id = static_cast<uint32_t>(b);
            batches_[b].rays.reserve(rays_per_batch_);

            // Generate a bundle of primary rays through a grid
            float u_base = static_cast<float>(b % 4) * 0.25f;
            float v_base = static_cast<float>(b / 4) * 0.25f;

            for (size_t r = 0; r < rays_per_batch_; ++r) {
                float du = static_cast<float>(r % 8) * (0.25f / 8.0f);
                float dv = static_cast<float>(r / 8) * (0.25f / 8.0f);
                float u = (u_base + du) * 2.0f - 1.0f;
                float v = (v_base + dv) * 2.0f - 1.0f;

                Vec3 target(u, v, 0.0f);
                Vec3 dir = (target - scene_.camera_pos).normalized();
                batches_[b].rays.emplace_back(scene_.camera_pos, dir);
            }
        }

        // Register versioned resources in TWRGraph
        auto& cam_res = graph_->add_resource(RAY_CAMERA_RESOURCE_ID, "RayCamera");
        cam_res.set_value(scene_.camera_pos);

        auto& geom_res = graph_->add_resource(RAY_GEOMETRY_RESOURCE_ID, "RayGeometry");
        geom_res.set_value(static_cast<uint32_t>(scene_.spheres.size()));

        auto& light_res = graph_->add_resource(RAY_LIGHT_RESOURCE_ID, "RayLight");
        light_res.set_value(scene_.light.position);

        // Register Batch TWRs
        for (size_t b = 0; b < batch_count_; ++b) {
            TWRId twr_id = RAY_BATCH_TWR_BASE + static_cast<TWRId>(b);
            auto& twr = graph_->add_twr(twr_id, "RayBatch_" + std::to_string(b));

            graph_->bind_resource(twr_id, RAY_CAMERA_RESOURCE_ID);
            graph_->bind_resource(twr_id, RAY_GEOMETRY_RESOURCE_ID);
            graph_->bind_resource(twr_id, RAY_LIGHT_RESOURCE_ID);

            twr.set_kernel([this, b](TemporalWorkRegion& self, LogicalStateStore& store,
                                     const std::vector<const VersionedResource*>&) -> bool {
                const auto& batch = batches_[b];
                RayBatchPayload payload;
                payload.batch_id = batch.batch_id;
                payload.ray_count = static_cast<uint32_t>(batch.rays.size());
                payload.hit_distances.resize(batch.rays.size());
                payload.hit_normals.resize(batch.rays.size());
                payload.occlusions.resize(batch.rays.size());

                for (size_t i = 0; i < batch.rays.size(); ++i) {
                    RayHit hit = scene_.trace_ray(batch.rays[i]);
                    if (hit.hit) {
                        payload.hit_distances[i] = hit.t;
                        payload.hit_normals[i] = hit.normal;

                        // Cast shadow ray
                        Vec3 shadow_dir = (scene_.light.position - hit.point).normalized();
                        float light_dist = (scene_.light.position - hit.point).length();
                        Ray shadow_ray(hit.point + hit.normal * 0.001f, shadow_dir, 0.001f, light_dist);

                        payload.occlusions[i] = scene_.is_occluded(shadow_ray) ? 1.0f : 0.0f;
                    } else {
                        payload.hit_distances[i] = -1.0f;
                        payload.hit_normals[i] = Vec3(0, 0, 0);
                        payload.occlusions[i] = 0.0f;
                    }
                }

                // Write packed payload to State Store
                size_t sz = sizeof(uint32_t) * 2 +
                            payload.hit_distances.size() * sizeof(float) +
                            payload.hit_normals.size() * sizeof(Vec3) +
                            payload.occlusions.size() * sizeof(float);

                std::vector<uint8_t> raw_buf(sz);
                size_t offset = 0;
                std::memcpy(raw_buf.data() + offset, &payload.batch_id, sizeof(uint32_t)); offset += sizeof(uint32_t);
                std::memcpy(raw_buf.data() + offset, &payload.ray_count, sizeof(uint32_t)); offset += sizeof(uint32_t);
                std::memcpy(raw_buf.data() + offset, payload.hit_distances.data(), payload.hit_distances.size() * sizeof(float));
                offset += payload.hit_distances.size() * sizeof(float);
                std::memcpy(raw_buf.data() + offset, payload.hit_normals.data(), payload.hit_normals.size() * sizeof(Vec3));
                offset += payload.hit_normals.size() * sizeof(Vec3);
                std::memcpy(raw_buf.data() + offset, payload.occlusions.data(), payload.occlusions.size() * sizeof(float));

                store.write_output(self.id(), raw_buf.data(), raw_buf.size(), self.current_output_version() + 1);
                return true;
            });
        }

        graph_->validate_and_compute_depths();
    }

    void notify_light_moved(const Vec3& new_pos) {
        scene_.light.position = new_pos;
        auto* res = graph_->get_resource(RAY_LIGHT_RESOURCE_ID);
        if (res) {
            res->set_value(new_pos);
        }
    }

    void notify_geometry_changed() {
        auto* res = graph_->get_resource(RAY_GEOMETRY_RESOURCE_ID);
        if (res) {
            res->set_value(static_cast<uint32_t>(scene_.spheres.size()));
            res->bump_version();
        }
    }

    void notify_camera_moved(const Vec3& new_pos) {
        scene_.camera_pos = new_pos;
        auto* res = graph_->get_resource(RAY_CAMERA_RESOURCE_ID);
        if (res) {
            res->set_value(new_pos);
        }
    }

    uint64_t run_step() {
        uint64_t exec_before = metrics_.total_twr_executions;
        scheduler_.run_frame(*graph_, state_store_, trace_, metrics_);
        return metrics_.total_twr_executions - exec_before;
    }

    // Reference oracle re-traces all ray batches unconditionally
    std::vector<RayBatchPayload> trace_all_forced() const {
        std::vector<RayBatchPayload> results(batch_count_);
        for (size_t b = 0; b < batch_count_; ++b) {
            const auto& batch = batches_[b];
            auto& payload = results[b];
            payload.batch_id = batch.batch_id;
            payload.ray_count = static_cast<uint32_t>(batch.rays.size());
            payload.hit_distances.resize(batch.rays.size());
            payload.hit_normals.resize(batch.rays.size());
            payload.occlusions.resize(batch.rays.size());

            for (size_t i = 0; i < batch.rays.size(); ++i) {
                RayHit hit = scene_.trace_ray(batch.rays[i]);
                if (hit.hit) {
                    payload.hit_distances[i] = hit.t;
                    payload.hit_normals[i] = hit.normal;

                    Vec3 shadow_dir = (scene_.light.position - hit.point).normalized();
                    float light_dist = (scene_.light.position - hit.point).length();
                    Ray shadow_ray(hit.point + hit.normal * 0.001f, shadow_dir, 0.001f, light_dist);

                    payload.occlusions[i] = scene_.is_occluded(shadow_ray) ? 1.0f : 0.0f;
                } else {
                    payload.hit_distances[i] = -1.0f;
                    payload.hit_normals[i] = Vec3(0, 0, 0);
                    payload.occlusions[i] = 0.0f;
                }
            }
        }
        return results;
    }

private:
    size_t batch_count_{16};
    size_t rays_per_batch_{64};
    RayScene scene_;
    std::vector<RayBatch> batches_;

    std::unique_ptr<TWRGraph> graph_;
    LogicalStateStore state_store_;
    TWRScheduler scheduler_;
    ExecutionTrace trace_;
    MetricsCollector metrics_;
};

} // namespace twrf::ray
