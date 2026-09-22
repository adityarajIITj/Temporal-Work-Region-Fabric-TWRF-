#include "twrf/raster/renderer.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S2-01: Static Frame Reuse & Bitwise Oracle Parity");

    twrf::raster::TileConfig cfg;
    cfg.frame_width = 128;
    cfg.frame_height = 128;
    cfg.tile_size = 16; // 8x8 = 64 tiles

    twrf::raster::TWRFRenderer renderer(cfg);

    // Setup scene with a centered colored triangle
    auto& sc = renderer.scene();
    sc.camera.position = twrf::raster::Vec3(0.0f, 0.0f, 2.5f);
    sc.camera.target = twrf::raster::Vec3(0.0f, 0.0f, 0.0f);

    sc.add_object("MainTriangle", twrf::raster::Mesh::create_triangle(0.8f));
    renderer.initialize();

    // Frame 1: Initial Render
    auto res1 = renderer.render_frame_incremental();
    TWRF_ASSERT(res1.tiles_executed > 0, "Frame 1 must execute tiles");

    // Compare Frame 1 against forced full recomputation oracle
    auto oracle1 = renderer.render_frame_forced_recompute();
    TWRF_ASSERT(res1.framebuffer.is_bitwise_identical(oracle1),
                "S2-01 Failed: Frame 1 does not bitwise match full recomputation oracle");

    // Frame 2: Completely static (no scene mutations)
    auto res2 = renderer.render_frame_incremental();

    TWRF_ASSERT(res2.tiles_executed == 0,
                "S2-01 Failed: Clean static frame must execute 0 tiles (all skipped)");
    TWRF_ASSERT(res2.tiles_skipped == static_cast<uint64_t>(cfg.total_tiles()),
                "S2-01 Failed: All tiles must be skipped on static frame");
    TWRF_ASSERT(res2.skip_ratio == 1.0, "Skip ratio on static frame must be 100%");

    // Verify output image of Frame 2 is still bitwise identical to Frame 1 and to oracle
    TWRF_ASSERT(res2.framebuffer.is_bitwise_identical(res1.framebuffer),
                "S2-01 Failed: Frame 2 reused framebuffer mutated unexpectedly");
    TWRF_ASSERT(res2.framebuffer.is_bitwise_identical(oracle1),
                "S2-01 Failed: Frame 2 reused framebuffer does not match oracle");

    TWRF_TEST_PASS("S2-01: Static Frame Reuse & Bitwise Oracle Parity");
    return 0;
}
