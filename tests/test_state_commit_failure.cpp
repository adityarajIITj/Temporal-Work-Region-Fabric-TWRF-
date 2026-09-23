#include "twrf/core/graph.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/state_store.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("P0: State Store commit failure invalidates execution");

    twrf::TWRGraph graph;
    twrf::LogicalStateStore store(sizeof(int) - 1);
    twrf::TWRScheduler scheduler;
    twrf::ExecutionTrace trace;
    twrf::MetricsCollector metrics;

    auto& input = graph.add_resource(1, "Input");
    input.set_value(11);

    auto& twr = graph.add_twr(10, "Writer");
    graph.bind_resource(twr.id(), input.id());

    int runs = 0;
    twr.set_kernel([&](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& s,
                       const std::vector<const twrf::VersionedResource*>& in) {
        ++runs;
        int value = in[0]->get_value<int>();
        // The store rejects this write because capacity is intentionally too small.
        return s.write_output(self.id(), &value, sizeof(value),
                              self.current_output_version() + 1);
    });

    TWRF_ASSERT(graph.validate_and_compute_depths(), "DAG validation failed");
    scheduler.run_frame(graph, store, trace, metrics);

    TWRF_ASSERT(runs == 1, "Writer should execute once");
    TWRF_ASSERT(!store.has_slot(twr.id()), "Rejected write must not create a committed slot");
    TWRF_ASSERT(twr.current_output_version() == twrf::INITIAL_VERSION,
                "Output version must not advance after failed State Store commit");
    TWRF_ASSERT(twr.status() == twrf::TWRStatus::Dirty,
                "TWR must remain dirty after State Store commit failure");

    TWRF_TEST_PASS("P0: State Store commit failure invalidates execution");
    return 0;
}
