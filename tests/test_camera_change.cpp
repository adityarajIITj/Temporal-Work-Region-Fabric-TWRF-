#include "twrf/raster/renderer.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S2-03: Moving Camera & Full Recomputation Oracle Match");

    twrf::raster::TileConfig cfg;
    cfg.frame_width = 128;
    cfg.frame_height = 128;
    cfg.tile_size = 16;

    twrf::raster::TWRFRenderer renderer(cfg);
    auto& sc = renderer.scene();

    // Add 3D cube
    sc.add_object("CenterCube", twrf::raster::Mesh::create_cube(0.5f));

    renderer.initialize();

    // Frame 1: Initial camera position
    renderer.render_frame_incremental();

    // Frame 2: Rotate / shift camera position
    sc.camera.position = twrf::raster::Vec3(1.5f, 1.0f, 3.0f);
    sc.camera.target = twrf::raster::Vec3(0.0f, 0.0f, 0.0f);
    renderer.notify_camera_changed();

    auto res2 = renderer.render_frame_incremental();
    auto oracle2 = renderer.render_frame_forced_recompute();

    TWRF_ASSERT(res2.framebuffer.is_bitwise_identical(oracle2),
                "S2-03 Failed: Camera move output does not match full recomputation oracle");

    // Frame 3: Move camera further
    sc.camera.position = twrf::raster::Vec3(-2.0f, 0.5f, 2.0f);
    renderer.notify_camera_changed();

    auto res3 = renderer.render_frame_incremental();
    auto oracle3 = renderer.render_frame_forced_recompute();

    TWRF_ASSERT(res3.framebuffer.is_bitwise_identical(oracle3),
                "S2-03 Failed: Second camera move output does not match oracle");

    TWRF_TEST_PASS("S2-03: Moving Camera & Full Recomputation Oracle Match");
    return 0;
}
