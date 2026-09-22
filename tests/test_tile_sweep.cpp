#include "twrf/raster/renderer.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S2-06: Configured Tile-Size Sweep (8x8, 16x16, 32x32)");

    int tile_sizes[] = {8, 16, 32};
    std::vector<twrf::raster::FrameBuffer> outputs;

    for (int ts : tile_sizes) {
        twrf::raster::TileConfig cfg;
        cfg.frame_width = 128;
        cfg.frame_height = 128;
        cfg.tile_size = ts;

        twrf::raster::TWRFRenderer renderer(cfg);
        auto& sc = renderer.scene();
        sc.camera.position = twrf::raster::Vec3(0.0f, 0.0f, 3.0f);
        sc.camera.target = twrf::raster::Vec3(0.0f, 0.0f, 0.0f);

        sc.add_object("Shape1", twrf::raster::Mesh::create_triangle(0.7f),
                      twrf::raster::Mat4::translation(-0.3f, 0.0f, 0.0f));
        sc.add_object("Shape2", twrf::raster::Mesh::create_quad(0.5f, 0.5f),
                      twrf::raster::Mat4::translation(0.4f, 0.2f, -0.2f));

        renderer.initialize();

        // Frame 1
        renderer.render_frame_incremental();

        // Frame 2: Shift shape slightly
        sc.set_object_transform(1, twrf::raster::Mat4::translation(-0.35f, 0.05f, 0.0f));
        renderer.notify_object_transform_changed(1);

        auto res2 = renderer.render_frame_incremental();
        auto oracle2 = renderer.render_frame_forced_recompute();

        // Invariant 1: Incremental must match oracle for each tile size
        TWRF_ASSERT(res2.framebuffer.is_bitwise_identical(oracle2),
                    "S2-06 Failed: Incremental does not match oracle for tile size " + std::to_string(ts));

        outputs.push_back(res2.framebuffer);
    }

    // Invariant 2: Across tile sizes 8x8, 16x16, and 32x32, the rendered images must be identical
    for (size_t i = 1; i < outputs.size(); ++i) {
        TWRF_ASSERT(outputs[i].is_bitwise_identical(outputs[0]),
                    "S2-06 Failed: Image output varies across tile sizes " +
                    std::to_string(tile_sizes[i]) + " vs " + std::to_string(tile_sizes[0]));
    }

    TWRF_TEST_PASS("S2-06: Configured Tile-Size Sweep (8x8, 16x16, 32x32)");
    return 0;
}
