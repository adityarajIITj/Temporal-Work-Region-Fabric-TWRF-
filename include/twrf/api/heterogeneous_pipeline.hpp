#pragma once

#include "twrf/core/types.hpp"
#include "twrf/core/twr.hpp"
#include "twrf/core/graph.hpp"
#include "twrf/core/state_store.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/trace.hpp"
#include "twrf/core/metrics.hpp"
#include "twrf/raster/math.hpp"
#include "twrf/ray/ray_types.hpp"
#include "twrf/neural/tensor.hpp"
#include "twrf/neural/mlp.hpp"
#include <vector>
#include <memory>
#include <cmath>
#include <iostream>

namespace twrf::api {

using raster::Vec3;
using raster::Vec2;
using ray::Ray;
using ray::RayHit;
using ray::Sphere;
using neural::Tensor1D;
using neural::MLPLayer;
using neural::MLPNetwork;

constexpr ResourceId HET_CAMERA_RES_ID = 100;
constexpr ResourceId HET_GEOMETRY_RES_ID = 101;
constexpr ResourceId HET_LIGHT_RES_ID = 102;
constexpr ResourceId HET_WEIGHTS_RES_ID = 103;

constexpr TWRId HET_RASTER_TWR_BASE = 1000;
constexpr TWRId HET_RAY_TWR_BASE = 2000;
constexpr TWRId HET_NEURAL_TWR_BASE = 3000;

struct GBufferTile {
    float depth{1.0f};
    Vec3 normal{0.0f, 1.0f, 0.0f};
    Vec3 albedo{0.8f, 0.8f, 0.8f};
};

struct RayTileOutput {
    float occlusion{0.0f}; // 0.0 = lit, 1.0 = shadowed
    Vec3 hit_point{0.0f, 0.0f, 0.0f};
};

struct ShadedTileOutput {
    Vec3 final_color{0.0f, 0.0f, 0.0f};
    float temporal_confidence{1.0f};
};

class HeterogeneousPipeline {
public:
    explicit HeterogeneousPipeline(size_t tile_count = 4) : tile_count_(tile_count) {
        graph_ = std::make_unique<TWRGraph>();

        // Build a 2-layer MLP for Stage 3 Neural Filter:
        // Input: [depth, normal(3), albedo(3), occlusion(1), prev_confidence(1)] = 9 dims
        // Output: [final_color(3), updated_confidence(1)] = 4 dims
        denoiser_model_.add_layer(MLPLayer(9, 8, true, 0.1f));
        denoiser_model_.add_layer(MLPLayer(8, 4, false, 0.1f));

        light_pos_ = Vec3(2.0f, 5.0f, 2.0f);
        camera_pos_ = Vec3(0.0f, 0.0f, 5.0f);
        sphere_pos_ = Vec3(0.0f, 0.0f, 0.0f);
    }

    [[nodiscard]] TWRGraph& graph() noexcept { return *graph_; }
    [[nodiscard]] LogicalStateStore& state_store() noexcept { return state_store_; }
    [[nodiscard]] const MetricsCollector& metrics() const noexcept { return metrics_; }
    [[nodiscard]] const ExecutionTrace& trace() const noexcept { return trace_; }

    void initialize() {
        // 1. External Versioned Resources
        auto& cam_res = graph_->add_resource(HET_CAMERA_RES_ID, "Camera");
        cam_res.set_value(camera_pos_);

        auto& geom_res = graph_->add_resource(HET_GEOMETRY_RES_ID, "Geometry");
        geom_res.set_value(sphere_pos_);

        auto& light_res = graph_->add_resource(HET_LIGHT_RES_ID, "Light");
        light_res.set_value(light_pos_);

        auto& weight_res = graph_->add_resource(HET_WEIGHTS_RES_ID, "NeuralWeights");
        auto w_bytes = denoiser_model_.serialize_weights();
        weight_res.set_data(w_bytes.data(), w_bytes.size());

        // 2. Multi-Stage Heterogeneous DAG
        for (size_t i = 0; i < tile_count_; ++i) {
            TWRId raster_id = HET_RASTER_TWR_BASE + static_cast<TWRId>(i);
            TWRId ray_id = HET_RAY_TWR_BASE + static_cast<TWRId>(i);
            TWRId neural_id = HET_NEURAL_TWR_BASE + static_cast<TWRId>(i);

            // STAGE 1: Raster G-Buffer TWR
            auto& twr_raster = graph_->add_twr(raster_id, "RasterGBuffer_" + std::to_string(i));
            graph_->bind_resource(raster_id, HET_CAMERA_RES_ID);
            graph_->bind_resource(raster_id, HET_GEOMETRY_RES_ID);

            twr_raster.set_kernel([this, i](TemporalWorkRegion& self, LogicalStateStore& store,
                                           const std::vector<const VersionedResource*>&) -> bool {
                self.observe_resource(HET_CAMERA_RES_ID);
                self.observe_resource(HET_GEOMETRY_RES_ID);
                GBufferTile gb;
                float tile_center_x = (static_cast<float>(i) - 1.5f) * 0.5f;
                Vec3 ray_dir = (Vec3(tile_center_x, 0.0f, 0.0f) - camera_pos_).normalized();
                Sphere sp{0, sphere_pos_, 1.0f, 0};
                Ray ray(camera_pos_, ray_dir);
                RayHit hit;

                if (sp.intersect(ray, hit)) {
                    gb.depth = hit.t;
                    gb.normal = hit.normal;
                    gb.albedo = Vec3(0.7f, 0.3f, 0.3f);
                } else {
                    gb.depth = 100.0f;
                    gb.normal = Vec3(0.0f, 1.0f, 0.0f);
                    gb.albedo = Vec3(0.1f, 0.1f, 0.15f);
                }

                if (!store.write_output(self.id(), &gb, sizeof(GBufferTile), self.current_output_version() + 1)) return false;


                return true;
            });

            // STAGE 2: Ray Shadow/Occlusion TWR (Downstream of Raster)
            auto& twr_ray = graph_->add_twr(ray_id, "RayOcclusion_" + std::to_string(i));
            graph_->bind_resource(ray_id, HET_LIGHT_RES_ID);
            graph_->bind_resource(ray_id, HET_GEOMETRY_RES_ID);
            graph_->connect_dependency(raster_id, ray_id); // Raster -> Ray dependency

            twr_ray.set_kernel([this, raster_id](TemporalWorkRegion& self, LogicalStateStore& store,
                                                 const std::vector<const VersionedResource*>&) -> bool {
                self.observe_resource(HET_LIGHT_RES_ID);
                self.observe_resource(HET_GEOMETRY_RES_ID);
                self.observe_upstream_producer(raster_id);
                // Read G-Buffer from producer Stage 1
                size_t sz = 0;
                VersionNumber ver = 0;
                const uint8_t* raw_gb = store.read_output(raster_id, sz, ver);
                if (!raw_gb || sz < sizeof(GBufferTile)) return false;

                const auto* gb = reinterpret_cast<const GBufferTile*>(raw_gb);
                RayTileOutput ray_out;

                if (gb->depth < 50.0f) {
                    Vec3 surface_pt = camera_pos_ + Vec3(0, 0, -gb->depth);
                    ray_out.hit_point = surface_pt;

                    // Trace shadow ray towards light
                    Vec3 shadow_dir = (light_pos_ - surface_pt).normalized();
                    float light_dist = (light_pos_ - surface_pt).length();
                    Ray shadow_ray(surface_pt + gb->normal * 0.001f, shadow_dir, 0.001f, light_dist);

                    Sphere sp{0, sphere_pos_, 1.0f, 0};
                    RayHit shadow_hit;
                    ray_out.occlusion = sp.intersect(shadow_ray, shadow_hit) ? 1.0f : 0.0f;
                } else {
                    ray_out.occlusion = 0.0f;
                }

                if (!store.write_output(self.id(), &ray_out, sizeof(RayTileOutput), self.current_output_version() + 1)) return false;


                return true;
            });

            // STAGE 3: Neural Filter TWR (Downstream of Ray)
            auto& twr_neural = graph_->add_twr(neural_id, "NeuralDenoise_" + std::to_string(i));
            graph_->bind_resource(neural_id, HET_WEIGHTS_RES_ID);
            graph_->connect_dependency(ray_id, neural_id); // Ray -> Neural dependency
            graph_->connect_dependency(raster_id, neural_id); // Neural directly consumes Raster G-Buffer

            twr_neural.set_kernel([this, raster_id, ray_id](TemporalWorkRegion& self, LogicalStateStore& store,
                                                           const std::vector<const VersionedResource*>&) -> bool {
                self.observe_resource(HET_WEIGHTS_RES_ID);
                self.observe_upstream_producer(raster_id);
                self.observe_upstream_producer(ray_id);
                // Read G-Buffer and Ray outputs
                size_t sz_gb = 0, sz_ray = 0, sz_prev = 0;
                VersionNumber v_gb = 0, v_ray = 0, v_prev = 0;

                const auto* gb = reinterpret_cast<const GBufferTile*>(store.read_output(raster_id, sz_gb, v_gb));
                const auto* ray_res = reinterpret_cast<const RayTileOutput*>(store.read_output(ray_id, sz_ray, v_ray));
                if (!gb || !ray_res) return false;

                // Read persistent temporal hidden state from previous frame
                float prev_confidence = 0.5f;
                const auto* prev_out = reinterpret_cast<const ShadedTileOutput*>(store.read_output(self.id(), sz_prev, v_prev));
                if (prev_out && sz_prev >= sizeof(ShadedTileOutput)) {
                    prev_confidence = prev_out->temporal_confidence;
                }

                // Construct input feature vector [depth, normal(3), albedo(3), occlusion(1), prev_conf(1)]
                Tensor1D feat(9);
                feat[0] = gb->depth * 0.1f;
                feat[1] = gb->normal.x; feat[2] = gb->normal.y; feat[3] = gb->normal.z;
                feat[4] = gb->albedo.x; feat[5] = gb->albedo.y; feat[6] = gb->albedo.z;
                feat[7] = ray_res->occlusion;
                feat[8] = prev_confidence;

                // Execute MLP forward pass
                Tensor1D out_vec = denoiser_model_.forward(feat);

                ShadedTileOutput final_out;
                final_out.final_color = Vec3(out_vec[0], out_vec[1], out_vec[2]);
                final_out.temporal_confidence = std::clamp(out_vec[3], 0.0f, 1.0f);

                if (!store.write_output(self.id(), &final_out, sizeof(ShadedTileOutput), self.current_output_version() + 1)) return false;


                return true;
            });
        }

        for (const auto& [id, twr] : graph_->twrs()) twr->enable_dependency_audit(true);
        graph_->validate_and_compute_depths();
    }

    void notify_light_moved(const Vec3& new_pos) {
        light_pos_ = new_pos;
        auto* res = graph_->get_resource(HET_LIGHT_RES_ID);
        if (res) res->set_value(new_pos);
    }

    void notify_geometry_moved(const Vec3& new_pos) {
        sphere_pos_ = new_pos;
        auto* res = graph_->get_resource(HET_GEOMETRY_RES_ID);
        if (res) res->set_value(new_pos);
    }

    void notify_neural_weights_changed() {
        auto* res = graph_->get_resource(HET_WEIGHTS_RES_ID);
        if (res) {
            auto w_bytes = denoiser_model_.serialize_weights();
            res->set_data(w_bytes.data(), w_bytes.size());
        }
    }

    uint64_t run_frame() {
        uint64_t exec_before = metrics_.total_twr_executions;
        scheduler_.run_frame(*graph_, state_store_, trace_, metrics_);
        return metrics_.total_twr_executions - exec_before;
    }

    uint64_t run_frame_forced_recompute() {
        for (const auto& [id, twr] : graph_->twrs()) {
            twr->mark_dirty(ExecutionReason::ForcedRecompute);
        }
        uint64_t exec_before = metrics_.total_twr_executions;
        scheduler_.run_frame(*graph_, state_store_, trace_, metrics_);
        return metrics_.total_twr_executions - exec_before;
    }

    [[nodiscard]] ShadedTileOutput read_final_tile(size_t tile_idx) const {
        TWRId neural_id = HET_NEURAL_TWR_BASE + static_cast<TWRId>(tile_idx);
        size_t sz = 0;
        VersionNumber ver = 0;
        const uint8_t* raw = state_store_.read_output(neural_id, sz, ver);
        if (raw && sz >= sizeof(ShadedTileOutput)) {
            return *reinterpret_cast<const ShadedTileOutput*>(raw);
        }
        return ShadedTileOutput{};
    }

private:
    size_t tile_count_{4};
    Vec3 light_pos_;
    Vec3 camera_pos_;
    Vec3 sphere_pos_;
    MLPNetwork denoiser_model_;

    std::unique_ptr<TWRGraph> graph_;
    LogicalStateStore state_store_;
    TWRScheduler scheduler_;
    ExecutionTrace trace_;
    MetricsCollector metrics_;
};

} // namespace twrf::api
