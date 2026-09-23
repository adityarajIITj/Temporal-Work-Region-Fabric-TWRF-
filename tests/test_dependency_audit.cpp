#include "twrf/core/graph.hpp"
#include "twrf/core/state_store.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("P0: Dependency Audit detects undeclared mutable resource reads");

    twrf::TWRGraph graph;
    twrf::LogicalStateStore store;

    auto& declared = graph.add_resource(1, "Declared");
    auto& hidden = graph.add_resource(2, "Hidden");

    auto& twr = graph.add_twr(10, "AuditedTWR");
    graph.bind_resource(twr.id(), declared.id());
    twr.enable_dependency_audit(true);

    twr.set_kernel([&](twrf::TemporalWorkRegion& self,
                       twrf::LogicalStateStore&,
                       const std::vector<const twrf::VersionedResource*>&) {
        self.observe_resource(declared.id());
        self.observe_resource(hidden.id());
        return true;
    });

    TWRF_ASSERT(graph.validate_and_compute_depths(), "DAG validation failed");

    std::vector<const twrf::VersionedResource*> inputs{&declared};
    const bool ok = twr.execute(store, inputs, 1);

    TWRF_ASSERT(!ok, "Audit must reject an undeclared mutable read");
    TWRF_ASSERT(!twr.dependency_audit_passes(), "Audit should report failure");
    TWRF_ASSERT(twr.status() == twrf::TWRStatus::Failed,
                "Audit failure must invalidate execution");
    TWRF_ASSERT(twr.current_output_version() == twrf::INITIAL_VERSION,
                "Audit failure must not advance output version");

    TWRF_TEST_PASS("P0: Dependency Audit detects undeclared mutable resource reads");
    return 0;
}
