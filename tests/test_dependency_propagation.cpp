#include "twrf/core/types.hpp"
#include "twrf/core/graph.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/state_store.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S1-03: Dependency Chain Propagation (A -> B -> C)");

    twrf::TWRGraph graph;
    twrf::LogicalStateStore state_store;
    twrf::TWRScheduler scheduler;
    twrf::ExecutionTrace trace;
    twrf::MetricsCollector metrics;

    auto& root_res = graph.add_resource(1, "RootInput");
    root_res.set_value(100);

    std::vector<int> exec_order;

    // Node A
    auto& twr_a = graph.add_twr(1, "NodeA");
    graph.bind_resource(twr_a.id(), root_res.id());
    twr_a.set_kernel([&](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& store,
                         const std::vector<const twrf::VersionedResource*>& in) {
        exec_order.push_back(1);
        int val = in[0]->get_value<int>(0) + 1;
        store.write_output(self.id(), &val, sizeof(val), self.current_output_version() + 1, 1);
        return true;
    });

    // Node B (consumes A)
    auto& twr_b = graph.add_twr(2, "NodeB");
    graph.connect_dependency(twr_a.id(), twr_b.id());
    twr_b.set_kernel([&](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& store,
                         const std::vector<const twrf::VersionedResource*>&) {
        exec_order.push_back(2);
        size_t sz;
        twrf::VersionNumber ver;
        const uint8_t* a_out = store.read_output(1, sz, ver);
        int val = *reinterpret_cast<const int*>(a_out) * 2;
        store.write_output(self.id(), &val, sizeof(val), self.current_output_version() + 1, 1);
        return true;
    });

    // Node C (consumes B)
    auto& twr_c = graph.add_twr(3, "NodeC");
    graph.connect_dependency(twr_b.id(), twr_c.id());
    twr_c.set_kernel([&](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& store,
                         const std::vector<const twrf::VersionedResource*>&) {
        exec_order.push_back(3);
        size_t sz;
        twrf::VersionNumber ver;
        const uint8_t* b_out = store.read_output(2, sz, ver);
        int val = *reinterpret_cast<const int*>(b_out) + 5;
        store.write_output(self.id(), &val, sizeof(val), self.current_output_version() + 1, 1);
        return true;
    });

    TWRF_ASSERT(graph.validate_and_compute_depths(), "DAG validation failed");
    TWRF_ASSERT(twr_a.topological_depth() == 0, "Depth of A should be 0");
    TWRF_ASSERT(twr_b.topological_depth() == 1, "Depth of B should be 1");
    TWRF_ASSERT(twr_c.topological_depth() == 2, "Depth of C should be 2");

    // Frame 1: Initial run
    scheduler.run_frame(graph, state_store, trace, metrics);
    TWRF_ASSERT(exec_order.size() == 3, "All 3 nodes should execute in Frame 1");
    TWRF_ASSERT(exec_order[0] == 1 && exec_order[1] == 2 && exec_order[2] == 3,
                "Nodes must execute in topological order A -> B -> C");

    // Verify output of C = (100 + 1) * 2 + 5 = 207
    size_t sz;
    twrf::VersionNumber ver;
    const uint8_t* c_out = state_store.read_output(3, sz, ver);
    TWRF_ASSERT(*reinterpret_cast<const int*>(c_out) == 207, "Output computation incorrect");

    // Frame 2: Clean run (no mutation)
    exec_order.clear();
    scheduler.run_frame(graph, state_store, trace, metrics);
    TWRF_ASSERT(exec_order.empty(), "Zero nodes should execute on clean frame");

    // Frame 3: Mutate root resource -> must propagate down A -> B -> C
    exec_order.clear();
    root_res.set_value(200);
    scheduler.run_frame(graph, state_store, trace, metrics);

    TWRF_ASSERT(exec_order.size() == 3, "All 3 nodes should re-execute upon root mutation");
    TWRF_ASSERT(exec_order[0] == 1 && exec_order[1] == 2 && exec_order[2] == 3,
                "S1-03 Failed: Propagation order must strictly be A -> B -> C");

    // Verify updated output of C = (200 + 1) * 2 + 5 = 407
    c_out = state_store.read_output(3, sz, ver);
    TWRF_ASSERT(*reinterpret_cast<const int*>(c_out) == 407, "Output of propagated execution incorrect");

    TWRF_TEST_PASS("S1-03: Dependency Chain Propagation (A -> B -> C)");
    return 0;
}
