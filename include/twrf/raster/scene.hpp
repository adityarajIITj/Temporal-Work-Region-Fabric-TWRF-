#pragma once

#include "twrf/raster/math.hpp"
#include "twrf/raster/geometry.hpp"
#include "twrf/raster/texture.hpp"
#include <vector>
#include <string>
#include <memory>

namespace twrf::raster {

class Camera {
public:
    Vec3 position{0.0f, 0.0f, 3.0f};
    Vec3 target{0.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    float fov_deg{60.0f};
    float aspect{1.0f};
    float near_z{0.1f};
    float far_z{100.0f};

    [[nodiscard]] Mat4 view_matrix() const noexcept {
        return Mat4::look_at(position, target, up);
    }

    [[nodiscard]] Mat4 proj_matrix() const noexcept {
        return Mat4::perspective(fov_deg * PI / 180.0f, aspect, near_z, far_z);
    }

    [[nodiscard]] Mat4 view_proj_matrix() const noexcept {
        return proj_matrix() * view_matrix();
    }
};

struct SceneObject {
    uint32_t id{0};
    std::string name;
    Mesh mesh;
    Mat4 transform{Mat4::identity()};
    uint32_t texture_id{0};
    BoundingRegion screen_bounds{0, 0, 0, 0};
    BoundingRegion prev_screen_bounds{0, 0, 0, 0};

    BoundingRegion update_screen_bounds(const Mat4& vp, int screen_w, int screen_h) noexcept {
        prev_screen_bounds = screen_bounds;
        Mat4 mvp = vp * transform;
        screen_bounds = mesh.compute_screen_bounds(mvp, screen_w, screen_h);
        return screen_bounds;
    }
};

class Scene {
public:
    Camera camera;
    std::vector<SceneObject> objects;
    std::vector<Texture> textures;

    uint32_t add_texture(Texture tex) {
        textures.push_back(std::move(tex));
        return static_cast<uint32_t>(textures.size() - 1);
    }

    uint32_t add_object(std::string name, Mesh mesh, Mat4 transform = Mat4::identity(), uint32_t tex_id = 0) {
        uint32_t id = static_cast<uint32_t>(objects.size() + 1);
        objects.push_back(SceneObject{id, std::move(name), std::move(mesh), transform, tex_id, {0,0,0,0}, {0,0,0,0}});
        return id;
    }

    SceneObject* get_object(uint32_t id) noexcept {
        for (auto& obj : objects) {
            if (obj.id == id) return &obj;
        }
        return nullptr;
    }

    void set_object_transform(uint32_t id, const Mat4& transform) {
        auto* obj = get_object(id);
        if (obj) {
            obj->transform = transform;
        }
    }

    void update_all_screen_bounds(int screen_w, int screen_h) noexcept {
        Mat4 vp = camera.view_proj_matrix();
        for (auto& obj : objects) {
            obj.update_screen_bounds(vp, screen_w, screen_h);
        }
    }
};

} // namespace twrf::raster
