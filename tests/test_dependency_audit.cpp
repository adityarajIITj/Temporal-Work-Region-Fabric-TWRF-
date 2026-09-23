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

    int committed_value = 42;
    TWRF_ASSERT(store.write_output(twr.id(), &committed_value, sizeof(committed_value), 7, 0),
                "Failed to establish committed pre-existing output");

    twr.set_kernel([&](twrf::TemporalWorkRegion& self,
                       twrf::LogicalStateStore& s,
                       const std::vector<const twrf::VersionedResource*>&) {
        self.observe_resource(declared.id());
        self.observe_resource(hidden.id());

        int replacement_value = 99;
        return s.write_output(self.id(), &replacement_value, sizeof(replacement_value), 8, 1);
    });

    TWRF_ASSERT(graph.validate_and_compute_depths(), "DAG validation failed");

    std::vector<const twrf::VersionedResource*> inputs{&declared};
    const bool ok = twr.execute(store, inputs, 1);

    TWRF_ASSERT(!ok, "Audit must reject an undeclared mutable read");
    TWRF_ASSERT(!twr.dependency_audit_passes(), "Audit should report failure");
    TWRF_ASSERT(twr.status() == twrf::TWRStatus::Failed,
                "Audit failure must invalidate execution");
    TWRF_ASSERT(twr.current_output_version() == twrf::INITIAL_VERSION,
                "Audit failure must not advance TWR output version");

    size_t out_size = 0;
    twrf::VersionNumber out_version = twrf::INVALID_VERSION;
    const auto* restored = store.read_output(twr.id(), out_size, out_version);
    TWRF_ASSERT(restored != nullptr && out_size == sizeof(int),
                "Audit failure must preserve the previously committed output");
    TWRF_ASSERT(*reinterpret_cast<const int*>(restored) == committed_value,
                "Audit failure must roll back the replacement output");
    TWRF_ASSERT(out_version == 7,
                "Audit failure must preserve the previously committed output version");

    TWRF_TEST_PASS("P0: Dependency Audit detects undeclared mutable resource reads");
    return 0;
}
