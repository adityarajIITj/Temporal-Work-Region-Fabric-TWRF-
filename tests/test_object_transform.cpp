#include "twrf/raster/renderer.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S2-02: Single-Object Transform Isolation & Bounding Invalidation");

    twrf::raster::TileConfig cfg;
    cfg.frame_width = 128;
    cfg.frame_height = 128;
    cfg.tile_size = 16; // 8x8 = 64 tiles total

    twrf::raster::TWRFRenderer renderer(cfg);
    auto& sc = renderer.scene();
    sc.camera.position = twrf::raster::Vec3(0.0f, 0.0f, 4.0f);
    sc.camera.target = twrf::raster::Vec3(0.0f, 0.0f, 0.0f);

    // Object 1: Located in top-left quadrant (-1.0, 1.0)
    uint32_t obj1_id = sc.add_object("Obj1", twrf::raster::Mesh::create_quad(0.3f, 0.3f),
                                     twrf::raster::Mat4::translation(-1.0f, 1.0f, 0.0f));

    // Object 2: Located in bottom-right quadrant (1.0, -1.0)
    uint32_t obj2_id = sc.add_object("Obj2", twrf::raster::Mesh::create_quad(0.3f, 0.3f),
                                     twrf::raster::Mat4::translation(1.0f, -1.0f, 0.0f));

    renderer.initialize();

    // Frame 1: Initial render
    renderer.render_frame_incremental();

    // Frame 2: Mutate ONLY Object 1 slightly (shift by delta in top-left)
    twrf::raster::Mat4 new_trans = twrf::raster::Mat4::translation(-1.05f, 1.05f, 0.0f);
    sc.set_object_transform(obj1_id, new_trans);
    renderer.notify_object_transform_changed(obj1_id);

    auto res2 = renderer.render_frame_incremental();

    // Verification 1: Only a small subset of tiles executed
    TWRF_ASSERT(res2.tiles_executed > 0, "S2-02 Failed: Moved object should trigger tile executions");
    TWRF_ASSERT(res2.tiles_executed < static_cast<uint64_t>(cfg.total_tiles() / 2),
                "S2-02 Failed: Moving one localized object should only dirty a small fraction of tiles");
    TWRF_ASSERT(res2.tiles_skipped > 0, "S2-02 Failed: Unrelated tiles should be skipped");

    // Verification 2: Object 2's region (bottom-right) MUST be completely skipped!
    const auto& obj2 = *sc.get_object(obj2_id);
    for (int i = 0; i < cfg.total_tiles(); ++i) {
        twrf::BoundingRegion tb = cfg.get_tile_bounds(i);
        if (tb.overlaps(obj2.screen_bounds)) {
            twrf::TWRId tid = twrf::raster::TILE_TWR_BASE + i;
            // Trace check: tile must not have executed in Frame 2
            bool tile_executed_in_frame_2 = false;
            for (const auto& entry : renderer.trace().entries()) {
                if (entry.twr_id == tid && entry.executed && entry.step > 64) {
                    tile_executed_in_frame_2 = true;
                    break;
                }
            }
            TWRF_ASSERT(!tile_executed_in_frame_2,
                        "S2-02 Failed: Unrelated tile overlapping Object 2 executed unexpectedly!");
        }
    }

    // Verification 3: Output matches full-recompute oracle exactly
    auto oracle = renderer.render_frame_forced_recompute();
    TWRF_ASSERT(res2.framebuffer.is_bitwise_identical(oracle),
                "S2-02 Failed: Incremental image does not match oracle after object transform");

    TWRF_TEST_PASS("S2-02: Single-Object Transform Isolation & Bounding Invalidation");
    return 0;
}
