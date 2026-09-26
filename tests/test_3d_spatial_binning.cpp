#include "twrf/core/state_store_3d.hpp"
#include "twrf/raster/spatial_binner_3d.hpp"
#include "twrf/raster/dual_layer_compositor.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace twrf;
using namespace twrf::raster;

Mat4 make_perspective(float fov_deg, float aspect, float near_z, float far_z) {
    float fov_rad = fov_deg * PI / 180.0f;
    float tan_half_fov = std::tan(fov_rad * 0.5f);

    Mat4 proj{};
    proj.m[0] = 1.0f / (aspect * tan_half_fov);
    proj.m[5] = 1.0f / tan_half_fov;
    proj.m[10] = -(far_z + near_z) / (far_z - near_z);
    proj.m[11] = -1.0f;
    proj.m[14] = -(2.0f * far_z * near_z) / (far_z - near_z);
    proj.m[15] = 0.0f;
    return proj;
}

void test_aabb_projection() {
    std::cout << "[TEST] 3D Bounding-Box Screen Projection... ";

    AABB3D box{Vec3{-1.0f, -1.0f, -1.0f}, Vec3{1.0f, 1.0f, 1.0f}};
    Mat4 proj = make_perspective(60.0f, 640.0f / 480.0f, 0.1f, 100.0f);
    Mat4 model = Mat4::translation(0.0f, 0.0f, -5.0f);
    Mat4 mvp = proj * model;

    auto screen_region = project_aabb_to_screen(box, mvp, 640, 480);
    assert(screen_region.has_value());
    assert(screen_region->min_x > 0 && screen_region->max_x < 640);
    assert(screen_region->min_y > 0 && screen_region->max_y < 480);
    assert(screen_region->max_x > screen_region->min_x);
    assert(screen_region->max_y > screen_region->min_y);

    std::cout << "PASSED (Screen Bounding Box: [" 
              << screen_region->min_x << ", " << screen_region->min_y << "] to [" 
              << screen_region->max_x << ", " << screen_region->max_y << "])\n";
}

void test_spatial_binner_static_scene() {
    std::cout << "[TEST] 3D Spatial Binner: Static Scene Zero Invalidation... ";

    TileConfig config{640, 480, 16};
    SpatialBinner3D binner(config);

    AABB3D car_box{Vec3{-1.5f, -0.8f, -2.5f}, Vec3{1.5f, 0.8f, 2.5f}};
    Mat4 car_world = Mat4::translation(0.0f, 0.0f, -10.0f);
    binner.register_entity(101, "Kuruma_Sedan", car_box, car_world);

    Mat4 proj = make_perspective(60.0f, 640.0f / 480.0f, 0.1f, 100.0f);

    // Frame 1: Cold start marks all moved
    binner.evaluate_frame(proj, false);

    // Frame 2: Entity is static, camera is static
    binner.evaluate_frame(proj, false);

    assert(binner.dirty_tile_count() == 0);
    assert(binner.clean_tile_count() == static_cast<uint32_t>(config.total_tiles()));
    assert(binner.skip_ratio() == 1.0);

    std::cout << "PASSED (100% Tiles Skipped on Static Frame)\n";
}

void test_spatial_binner_moving_entity() {
    std::cout << "[TEST] 3D Spatial Binner: Moving Vehicle Localized Invalidation... ";

    TileConfig config{640, 480, 16};
    SpatialBinner3D binner(config);

    AABB3D car_box{Vec3{-1.0f, -0.8f, -2.0f}, Vec3{1.0f, 0.8f, 2.0f}};
    Mat4 car_world_f1 = Mat4::translation(0.0f, 0.0f, -8.0f);
    binner.register_entity(101, "Police_Cruiser", car_box, car_world_f1);

    Mat4 proj = make_perspective(60.0f, 640.0f / 480.0f, 0.1f, 100.0f);
    binner.evaluate_frame(proj, false); // Clear cold state

    // Frame 2: Move car slightly along X axis (visible within 60-degree FOV at Z=-8)
    Mat4 car_world_f2 = Mat4::translation(1.0f, 0.0f, -8.0f);
    binner.update_entity_transform(101, car_world_f2);
    binner.evaluate_frame(proj, false);

    uint32_t dirty = binner.dirty_tile_count();
    uint32_t total = config.total_tiles();
    double skip = binner.skip_ratio();

    assert(dirty > 0);
    assert(dirty < total);
    assert(skip > 0.85); // Over 85% of city background tiles remain cached!

    std::cout << "PASSED (Dirty Tiles: " << dirty << " / " << total 
              << " | Temporal Reuse: " << (skip * 100.0) << "%)\n";
}

void test_deep_state_store_z_occlusion() {
    std::cout << "[TEST] Deep 3D State Store: Z-Buffer Occlusion Query... ";

    LogicalStateStore3D store;
    store.allocate_tile(42, 10, 5, 16, 16);

    // Fill tile with static skyscraper at depth Z = 0.5f
    std::vector<ColorRGBA> colors(256, ColorRGBA(0.4f, 0.4f, 0.5f));
    std::vector<float> depths(256, 0.5f);

    store.write_tile(42, colors.data(), depths.data(), 256, 1, 1);

    // Test 1: Pedestrian behind the building (Z = 0.7f) -> Must be occluded!
    bool occluded_behind = store.is_depth_occluded(42, 0.7f);
    assert(occluded_behind == true);

    // Test 2: Car in front of the building (Z = 0.3f) -> Must NOT be occluded!
    bool occluded_front = store.is_depth_occluded(42, 0.3f);
    assert(occluded_front == false);

    assert(store.metrics().depth_occlusion_tests == 2);
    assert(store.metrics().depth_occlusion_culls == 1);

    std::cout << "PASSED (Depth Occlusion Correctly Culled Hidden Object)\n";
}

void test_dual_layer_hud() {
    std::cout << "[TEST] Dual-Layer Compositor: Minimap and Status Overlay... ";

    TileConfig config{640, 480, 16};
    DualLayerCompositor comp(config);

    // Register GTA III Minimap at bottom left: [16, 360] to [112, 456]
    comp.register_hud_element("Radar_Minimap", 16, 360, 112, 456);
    // Register Health & Armor at top right: [500, 20] to [620, 60]
    comp.register_hud_element("Health_Armor", 500, 20, 620, 60);

    assert(comp.total_hud_tiles() > 0);

    // Cold frame
    comp.finish_frame();

    // Frame 2: Claude takes damage! Only Health element is marked dirty
    comp.mark_hud_element_dirty("Health_Armor");

    // Check tile in health region (x = 550, y = 30 -> tx = 34, ty = 1)
    int health_tile = 1 * config.tiles_x() + 34;
    assert(comp.is_hud_tile(health_tile));
    assert(comp.is_hud_tile_dirty(health_tile) == true);

    // Check tile in minimap region (x = 50, y = 400 -> tx = 3, ty = 25)
    int minimap_tile = 25 * config.tiles_x() + 3;
    assert(comp.is_hud_tile(minimap_tile));
    assert(comp.is_hud_tile_dirty(minimap_tile) == false); // Minimap was NOT damaged!

    std::cout << "PASSED (Selective HUD Element Invalidation Verified)\n";
}

int main() {
    std::cout << "========================================================================\n"
              << "       TWRF-GTA3: 3D SPATIAL BINNING & DEEP STATE STORE ACCEPTANCE      \n"
              << "========================================================================\n";

    test_aabb_projection();
    test_spatial_binner_static_scene();
    test_spatial_binner_moving_entity();
    test_deep_state_store_z_occlusion();
    test_dual_layer_hud();

    std::cout << "========================================================================\n"
              << "                 ALL 3D ACCEPTANCE TESTS PASSED (5/5)                   \n"
              << "========================================================================\n";
    return 0;
}
