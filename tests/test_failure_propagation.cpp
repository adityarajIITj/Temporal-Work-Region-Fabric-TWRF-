#include "twrf/core/graph.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/state_store.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("P0: Failed producer cannot release downstream consumers");

    twrf::TWRGraph graph;
    twrf::LogicalStateStore store;
    twrf::TWRScheduler scheduler;
    twrf::ExecutionTrace trace;
    twrf::MetricsCollector metrics;

    auto& input = graph.add_resource(1, "Input");
    input.set_value(7);

    auto& producer = graph.add_twr(10, "Producer");
    auto& consumer = graph.add_twr(20, "Consumer");
    graph.bind_resource(producer.id(), input.id());
    graph.connect_dependency(producer.id(), consumer.id());

    int producer_runs = 0;
    int consumer_runs = 0;

    producer.set_kernel([&](twrf::TemporalWorkRegion&, twrf::LogicalStateStore&,
                            const std::vector<const twrf::VersionedResource*>&) {
        ++producer_runs;
        return false; // deterministic execution failure
    });

    consumer.set_kernel([&](twrf::TemporalWorkRegion&, twrf::LogicalStateStore&,
                            const std::vector<const twrf::VersionedResource*>&) {
        ++consumer_runs;
        return true;
    });

    TWRF_ASSERT(graph.validate_and_compute_depths(), "DAG validation failed");
    scheduler.run_frame(graph, store, trace, metrics);

    TWRF_ASSERT(producer_runs == 1, "Producer should execute once");
    TWRF_ASSERT(consumer_runs == 0, "Consumer must not execute after producer failure");
    TWRF_ASSERT(producer.status() == twrf::TWRStatus::Dirty,
                "Failed producer must remain dirty/retryable");
    TWRF_ASSERT(consumer.status() == twrf::TWRStatus::Dirty,
                "Consumer must remain dirty while producer has not succeeded");

    TWRF_TEST_PASS("P0: Failed producer cannot release downstream consumers");
    return 0;
}
