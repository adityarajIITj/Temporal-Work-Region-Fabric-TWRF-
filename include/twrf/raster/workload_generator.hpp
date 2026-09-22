#pragma once

#include "twrf/raster/renderer.hpp"
#include <vector>
#include <cmath>

namespace twrf::raster {

enum class LocalityPattern {
    Clustered, // Single concentrated movement area
    Dispersed  // Multiple scattered moving items
};

struct SceneBenchmarkConfig {
    double change_rate{0.0}; // 0.0 to 1.0 (0%, 5%, ..., 100%)
    LocalityPattern locality{LocalityPattern::Clustered};
    int total_objects{16};
    int frame_count{10};
};

class WorkloadGenerator {
public:
    static void setup_benchmark_scene(TWRFRenderer& renderer, int object_count = 16) {
        auto& sc = renderer.scene();
        sc.objects.clear();
        sc.textures.clear();

        // Add 2 textures
        uint32_t t0 = sc.add_texture(Texture::create_checkerboard(32, 32, 4, ColorRGBA::white(), ColorRGBA{100, 100, 200, 255}));
        uint32_t t1 = sc.add_texture(Texture::create_checkerboard(32, 32, 4, ColorRGBA::red(), ColorRGBA::green()));

        // Set camera
        sc.camera.position = Vec3(0.0f, 0.0f, 5.0f);
        sc.camera.target = Vec3(0.0f, 0.0f, 0.0f);
        sc.camera.fov_deg = 50.0f;

        // Arrange objects in a grid
        int side = static_cast<int>(std::ceil(std::sqrt(object_count)));
        float spacing = 1.6f / std::max(1, side);

        for (int i = 0; i < object_count; ++i) {
            int gx = i % side;
            int gy = i / side;
            float ox = -0.8f + gx * spacing + spacing * 0.5f;
            float oy = -0.8f + gy * spacing + spacing * 0.5f;

            Mesh quad = Mesh::create_quad(spacing * 0.4f, spacing * 0.4f, (i % 2 == 0) ? t0 : t1);
            Mat4 trans = Mat4::translation(ox, oy, 0.0f);
            sc.add_object("Obj_" + std::to_string(i), std::move(quad), trans, (i % 2 == 0) ? t0 : t1);
        }
    }

    // Applies controlled mutation according to change rate p in [0, 1]
    static void apply_change_step(TWRFRenderer& renderer, double p, int frame_step,
                                 LocalityPattern locality = LocalityPattern::Clustered) {
        auto& sc = renderer.scene();
        if (p <= 0.0) return; // 0% dynamic

        if (p >= 1.0) {
            // 100% dynamic: rotate camera and invalidate all tiles
            float angle = frame_step * 0.02f;
            sc.camera.position = Vec3(std::sin(angle) * 5.0f, 0.0f, std::cos(angle) * 5.0f);
            renderer.notify_camera_changed();
            for (int i = 0; i < renderer.config().total_tiles(); ++i) {
                TWRId tid = raster::TILE_TWR_BASE + i;
                auto* twr = renderer.graph().get_twr(tid);
                if (twr) {
                    twr->mark_dirty(ExecutionReason::ForcedRecompute);
                }
            }
            return;
        }

        int total_objs = static_cast<int>(sc.objects.size());
        int objs_to_move = std::max(1, static_cast<int>(std::round(p * total_objs)));

        float delta = std::sin(frame_step * 0.1f) * 0.05f;

        if (locality == LocalityPattern::Clustered) {
            // Move adjacent objects
            for (int i = 0; i < objs_to_move && i < total_objs; ++i) {
                uint32_t id = sc.objects[i].id;
                Mat4 cur = sc.objects[i].transform;
                Mat4 mutated = Mat4::translation(delta, 0.0f, 0.0f) * cur;
                sc.set_object_transform(id, mutated);
                renderer.notify_object_transform_changed(id);
            }
        } else {
            // Move dispersed objects (stride through list)
            int stride = std::max(1, total_objs / objs_to_move);
            for (int k = 0; k < objs_to_move; ++k) {
                int idx = (k * stride) % total_objs;
                uint32_t id = sc.objects[idx].id;
                Mat4 cur = sc.objects[idx].transform;
                Mat4 mutated = Mat4::translation(delta, delta * 0.5f, 0.0f) * cur;
                sc.set_object_transform(id, mutated);
                renderer.notify_object_transform_changed(id);
            }
        }
    }
};

} // namespace twrf::raster
