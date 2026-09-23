#include "twrf/core/graph.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/state_store.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("P0: Direct consumer dependency must be declared");

    twrf::TWRGraph graph;
    twrf::LogicalStateStore store;
    twrf::TWRScheduler scheduler;
    twrf::ExecutionTrace trace;
    twrf::MetricsCollector metrics;

    auto& raster_input = graph.add_resource(1, "RasterInput");
    raster_input.set_value(1);

    auto& raster = graph.add_twr(10, "Raster");
    auto& ray = graph.add_twr(20, "Ray");
    auto& neural = graph.add_twr(30, "Neural");

    graph.bind_resource(raster.id(), raster_input.id());
    graph.connect_dependency(raster.id(), neural.id());

    int raster_runs = 0;
    int ray_runs = 0;
    int neural_runs = 0;

    raster.set_kernel([&](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& s,
                          const std::vector<const twrf::VersionedResource*>&) {
        ++raster_runs;
        int out = raster_runs;
        return s.write_output(self.id(), &out, sizeof(out),
                              self.current_output_version() + 1);
    });

    ray.set_kernel([&](twrf::TemporalWorkRegion&, twrf::LogicalStateStore&,
                       const std::vector<const twrf::VersionedResource*>&) {
        ++ray_runs;
        return true;
    });

    neural.set_kernel([&](twrf::TemporalWorkRegion&, twrf::LogicalStateStore&,
                          const std::vector<const twrf::VersionedResource*>&) {
        ++neural_runs;
        return true;
    });

    TWRF_ASSERT(graph.validate_and_compute_depths(), "DAG validation failed");
    scheduler.run_frame(graph, store, trace, metrics);

    TWRF_ASSERT(raster_runs == 1 && neural_runs == 1,
                "Initial direct dependency execution failed");

    // Simulate a Raster output mutation without changing the Ray node.
    raster.mark_dirty(twrf::ExecutionReason::ConservativeInvalidation);
    scheduler.run_frame(graph, store, trace, metrics);

    TWRF_ASSERT(raster_runs == 2, "Raster should re-execute");
    TWRF_ASSERT(ray_runs == 1, "Unrelated Ray node must remain clean");
    TWRF_ASSERT(neural_runs == 2,
                "Neural must re-execute because it directly consumes Raster output");

    TWRF_TEST_PASS("P0: Direct consumer dependency must be declared");
    return 0;
}
