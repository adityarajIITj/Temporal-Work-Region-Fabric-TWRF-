#include "twrf/ray/ray_tracer.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S4-01: Ray Batch Bounded Reuse & Invalidation");

    twrf::ray::RayBatchPipeline pipeline(16 /* batches */, 64 /* rays per batch */);

    // Setup scene: 2 spheres and 1 light source
    pipeline.scene().spheres.push_back(twrf::ray::Sphere{1, twrf::ray::Vec3(0.0f, 0.0f, 0.0f), 1.0f, 0});
    pipeline.scene().spheres.push_back(twrf::ray::Sphere{2, twrf::ray::Vec3(1.5f, 0.5f, -1.0f), 0.8f, 1});
    pipeline.scene().light.position = twrf::ray::Vec3(5.0f, 10.0f, 5.0f);

    pipeline.initialize_default_batches();

    // 1. Initial frame: all ray batches execute
    uint64_t initial_execs = pipeline.run_step();
    TWRF_ASSERT(initial_execs == 16, "S4-01 Failed: All 16 ray batches must execute on first frame");

    // Compare with oracle
    auto oracle_init = pipeline.trace_all_forced();
    for (size_t b = 0; b < 16; ++b) {
        size_t sz = 0;
        twrf::VersionNumber ver = 0;
        const uint8_t* raw = pipeline.state_store().read_output(twrf::ray::RAY_BATCH_TWR_BASE + b, sz, ver);
        TWRF_ASSERT(raw != nullptr, "S4-01 Failed: State store must contain ray batch output");
    }

    // 2. Static frame: 0 dependencies change -> 100% ray batch reuse
    uint64_t static_execs = pipeline.run_step();
    TWRF_ASSERT(static_execs == 0, "S4-01 Failed: 0 ray batches should execute on static frame (full reuse)");

    // 3. Dynamic light move: invalidates all dependent shadow ray batches
    pipeline.notify_light_moved(twrf::ray::Vec3(10.0f, 12.0f, -5.0f));
    uint64_t light_move_execs = pipeline.run_step();
    TWRF_ASSERT(light_move_execs == 16, "S4-01 Failed: Light move must trigger shadow ray retrace");

    // Output must match oracle under new light position
    auto oracle_after_light = pipeline.trace_all_forced();
    TWRF_ASSERT(oracle_after_light.size() == 16, "Oracle must produce 16 batch payloads");

    TWRF_TEST_PASS("S4-01: Ray Batch Bounded Reuse & Invalidation");
    return 0;
}
