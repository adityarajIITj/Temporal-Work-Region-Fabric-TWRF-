#include "twrf/api/heterogeneous_pipeline.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S4-04: Cross-Workload DAG Dependency Propagation (Raster -> Ray -> Neural)");

    twrf::api::HeterogeneousPipeline pipeline(4 /* 4 tiles */);
    pipeline.initialize();

    // 1. Frame 1: Cold start -> all 3 stages execute across all 4 tiles
    // 4 Raster TWRs + 4 Ray TWRs + 4 Neural TWRs = 12 executions
    uint64_t exec1 = pipeline.run_frame();
    TWRF_ASSERT(exec1 == 12, "S4-04 Failed: Cold start must execute all 12 heterogeneous TWRs");

    // 2. Frame 2: Completely static scene -> 100% reuse across all stages
    uint64_t exec2 = pipeline.run_frame();
    TWRF_ASSERT(exec2 == 0, "S4-04 Failed: Static frame must skip all 12 TWRs (100% cross-stage reuse)");

    // 3. Frame 3: Only Light moves
    // Invariant: Raster G-Buffer does NOT depend on light -> all 4 Raster TWRs must be SKIPPED!
    // Ray Shadow TWRs and downstream Neural TWRs must EXECUTE (4 Ray + 4 Neural = 8 executions)
    pipeline.notify_light_moved(twrf::raster::Vec3(10.0f, 15.0f, 5.0f));
    uint64_t exec3 = pipeline.run_frame();
    TWRF_ASSERT(exec3 == 8, "S4-04 Failed: Light move must skip Raster stage and execute only Ray + Neural stages");

    // 4. Frame 4: Only Neural model weights change
    // Invariant: Raster and Ray stages do NOT depend on neural weights -> both SKIPPED (0 execs)!
    // Only 4 Neural TWRs must EXECUTE!
    pipeline.notify_neural_weights_changed();
    uint64_t exec4 = pipeline.run_frame();
    TWRF_ASSERT(exec4 == 4, "S4-04 Failed: Neural weight change must skip Raster & Ray, executing only Neural stage");

    // 5. Frame 5: Geometry moves
    // Invariant: Geometry invalidates Raster -> propagates down to Ray and Neural -> all 12 EXECUTE!
    pipeline.notify_geometry_moved(twrf::raster::Vec3(0.5f, 0.2f, 0.0f));
    uint64_t exec5 = pipeline.run_frame();
    TWRF_ASSERT(exec5 == 12, "S4-04 Failed: Geometry movement must propagate downstream, invalidating all 12 TWRs");

    TWRF_TEST_PASS("S4-04: Cross-Workload DAG Dependency Propagation (Raster -> Ray -> Neural)");
    return 0;
}
