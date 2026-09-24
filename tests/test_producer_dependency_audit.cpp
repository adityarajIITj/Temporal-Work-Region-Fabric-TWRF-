#include "twrf/core/graph.hpp"
#include "twrf/core/state_store.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("P0: Dependency Audit validates producer-output declarations");

    twrf::TWRGraph graph;
    twrf::LogicalStateStore store;

    auto& producer = graph.add_twr(1, "Producer");
    auto& consumer = graph.add_twr(2, "Consumer");
    graph.connect_dependency(producer.id(), consumer.id());
    consumer.enable_dependency_audit(true);

    producer.set_kernel([](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& s,
                           const std::vector<const twrf::VersionedResource*>&) {
        int value = 42;
        return s.write_output(self.id(), &value, sizeof(value),
                              self.current_output_version() + 1);
    });

    consumer.set_kernel([&](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& s,
                            const std::vector<const twrf::VersionedResource*>&) {
        self.observe_upstream_producer(producer.id());
        size_t sz = 0;
        twrf::VersionNumber ver = 0;
        const auto* data = s.read_output(producer.id(), sz, ver);
        return data != nullptr && sz == sizeof(int);
    });

    TWRF_ASSERT(graph.validate_and_compute_depths(), "DAG validation failed");

    // The declaration is present through connect_dependency().
    TWRF_ASSERT(producer.execute(store, {}, 1), "Producer execution failed");
    TWRF_ASSERT(consumer.execute(store, {}, 1), "Consumer audit should pass");
    TWRF_ASSERT(consumer.dependency_audit_passes(),
                "Declared producer output read should pass audit");

    TWRF_TEST_PASS("P0: Dependency Audit validates producer-output declarations");
    return 0;
}
