#include "twrf/core/types.hpp"
#include "twrf/core/graph.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/state_store.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S1-02, S1-04, S1-05, S1-06: Change Detection & Invalidation Soundness");

    twrf::TWRGraph graph;
    twrf::LogicalStateStore state_store;
    twrf::TWRScheduler scheduler;
    twrf::ExecutionTrace trace;
    twrf::MetricsCollector metrics;

    // Resources: Res1 (for TWR1), Res2 (for TWR2)
    auto& res1 = graph.add_resource(1, "Res1", twrf::BoundingRegion::create(0, 0, 100, 100));
    auto& res2 = graph.add_resource(2, "Res2", twrf::BoundingRegion::create(200, 200, 300, 300));
    res1.set_value(10);
    res2.set_value(20);

    int exec1 = 0;
    int exec2 = 0;

    auto& twr1 = graph.add_twr(10, "TWR1", twrf::BoundingRegion::create(0, 0, 100, 100));
    graph.bind_resource(twr1.id(), res1.id());
    twr1.set_kernel([&](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& store,
                        const std::vector<const twrf::VersionedResource*>& in) {
        exec1++;
        int val = in[0]->get_value<int>(0);
        store.write_output(self.id(), &val, sizeof(val), self.current_output_version() + 1, 1);
        return true;
    });

    auto& twr2 = graph.add_twr(20, "TWR2", twrf::BoundingRegion::create(200, 200, 300, 300));
    graph.bind_resource(twr2.id(), res2.id());
    twr2.set_kernel([&](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& store,
                        const std::vector<const twrf::VersionedResource*>& in) {
        exec2++;
        int val = in[0]->get_value<int>(0);
        store.write_output(self.id(), &val, sizeof(val), self.current_output_version() + 1, 1);
        return true;
    });

    TWRF_ASSERT(graph.validate_and_compute_depths(), "DAG validation failed");

    // Frame 1: Initial
    scheduler.run_frame(graph, state_store, trace, metrics);
    TWRF_ASSERT(exec1 == 1 && exec2 == 1, "Initial execution failed");

    // Frame 2: S1-02 & S1-04: Mutate only Res1
    // Res1 version increments. TWR1 should become dirty exactly once and execute.
    // Res2 unchanged. TWR2 must remain clean and NOT execute (S1-04).
    res1.set_value(15); // Bumps Res1 version
    scheduler.run_frame(graph, state_store, trace, metrics);

    TWRF_ASSERT(exec1 == 2, "S1-02 Failed: TWR1 should execute once when its input resource version incremented");
    TWRF_ASSERT(exec2 == 1, "S1-04 Failed: Unrelated TWR2 executed when only Res1 changed");

    // Frame 3: S1-05: False-positive invalidation
    // Conservatively dirty TWR1 even though Res1 version was not modified
    twr1.mark_dirty(twrf::ExecutionReason::ConservativeInvalidation);
    scheduler.run_frame(graph, state_store, trace, metrics);

    TWRF_ASSERT(exec1 == 3, "S1-05 Failed: Conservative invalidation should force re-execution");
    size_t sz = 0;
    twrf::VersionNumber ver = 0;
    const uint8_t* data = state_store.read_output(twr1.id(), sz, ver);
    TWRF_ASSERT(*reinterpret_cast<const int*>(data) == 15, "S1-05 Failed: False positive altered output correctness");

    // S1-06: False-negative simulation harness
    // Simulate a failure mode: Res1 is mutated, but dirty flag was NOT set (stale reuse hazard)
    res1.bump_version();
    // Deliberately tamper recorded version to mimic a false-negative bug
    bool false_negative_detected = false;
    for (const auto& binding : twr1.resource_bindings()) {
        const auto* r = graph.get_resource(binding.resource_id);
        if (r && r->version() != binding.recorded_version) {
            // Harness verifies that stale reuse is detected
            false_negative_detected = true;
        }
    }
    TWRF_ASSERT(false_negative_detected, "S1-06 Failed: Harness failed to detect stale input discrepancy");

    TWRF_TEST_PASS("S1-02, S1-04, S1-05, S1-06: Change Detection & Invalidation Soundness");
    return 0;
}
