#include "twrf/ray/ray_tracer.hpp"
#include "tests/test_common.hpp"
#include <cmath>

int main() {
    TWRF_TEST_START("P0: Camera mutation refreshes derived ray inputs");

    twrf::ray::RayBatchPipeline pipeline(1, 1);
    pipeline.initialize_default_batches();

    const auto before = pipeline.batches()[0].rays[0];
    pipeline.notify_camera_moved(twrf::ray::Vec3(3.0f, 0.0f, 5.0f));
    const auto after = pipeline.batches()[0].rays[0];

    TWRF_ASSERT(std::abs(before.origin.x - 0.0f) < 1e-6f &&
                std::abs(before.origin.z - 5.0f) < 1e-6f,
                "Initial primary ray origin is incorrect");
    TWRF_ASSERT(std::abs(after.origin.x - 3.0f) < 1e-6f &&
                std::abs(after.origin.z - 5.0f) < 1e-6f,
                "Camera move must refresh derived ray origins");
    TWRF_ASSERT(std::abs(before.direction.x - after.direction.x) > 1e-6f ||
                std::abs(before.direction.y - after.direction.y) > 1e-6f ||
                std::abs(before.direction.z - after.direction.z) > 1e-6f,
                "Camera move must refresh derived ray directions");

    TWRF_TEST_PASS("P0: Camera mutation refreshes derived ray inputs");
    return 0;
}
