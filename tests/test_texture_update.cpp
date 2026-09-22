#include "twrf/raster/renderer.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S2-04: Texture Update Propagation & Stale Pixel Elimination");

    twrf::raster::TileConfig cfg;
    cfg.frame_width = 128;
    cfg.frame_height = 128;
    cfg.tile_size = 16;

    twrf::raster::TWRFRenderer renderer(cfg);
    auto& sc = renderer.scene();

    // Initial red texture
    uint32_t tex_id = sc.add_texture(twrf::raster::Texture(16, 16, twrf::raster::ColorRGBA::red()));

    // Quad using this texture
    sc.add_object("TexturedQuad", twrf::raster::Mesh::create_quad(0.6f, 0.6f, tex_id),
                  twrf::raster::Mat4::identity(), tex_id);

    renderer.initialize();

    // Frame 1: Render with red texture
    renderer.render_frame_incremental();

    // Verify initial pixels contain red
    const auto& fb1 = renderer.framebuffer();
    twrf::raster::ColorRGBA center1 = fb1.get_pixel(64, 64);
    TWRF_ASSERT(center1.r > 200 && center1.g < 50 && center1.b < 50,
                "Frame 1 should display red texture");

    // Frame 2: Mutate texture to bright GREEN
    sc.textures[tex_id] = twrf::raster::Texture(16, 16, twrf::raster::ColorRGBA::green());
    renderer.notify_texture_changed(tex_id);

    auto res2 = renderer.render_frame_incremental();
    auto oracle2 = renderer.render_frame_forced_recompute();

    // Invariant: Output must match oracle bit-for-bit
    TWRF_ASSERT(res2.framebuffer.is_bitwise_identical(oracle2),
                "S2-04 Failed: Texture update output does not match full-recompute oracle");

    // Invariant: Stale red pixel must not survive
    twrf::raster::ColorRGBA center2 = res2.framebuffer.get_pixel(64, 64);
    TWRF_ASSERT(center2.g > 200 && center2.r < 50,
                "S2-04 Failed: Stale texture survived; center pixel is not green");

    TWRF_TEST_PASS("S2-04: Texture Update Propagation & Stale Pixel Elimination");
    return 0;
}
