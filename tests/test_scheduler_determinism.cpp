#include "twrf/core/types.hpp"
#include "twrf/core/graph.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/state_store.hpp"
#include "tests/test_common.hpp"

// Build a diamond graph: Root -> (Branch1, Branch2) -> Sink
void build_and_run_graph(twrf::ExecutionTrace& out_trace) {
    twrf::TWRGraph graph;
    twrf::LogicalStateStore state_store;
    twrf::TWRScheduler scheduler;
    twrf::MetricsCollector metrics;

    auto& res = graph.add_resource(1, "InputData");
    res.set_value(50);

    auto& root = graph.add_twr(1, "RootNode", twrf::BoundingRegion::full_screen(), 0);
    graph.bind_resource(root.id(), res.id());
    root.set_kernel([](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& s, const auto& in) {
        int v = in[0]->template get_value<int>(0);
        s.write_output(self.id(), &v, sizeof(v), self.current_output_version() + 1, 1);
        return true;
    });

    // Branch 1 & Branch 2 share the exact same priority and topological depth
    auto& b1 = graph.add_twr(2, "BranchNode1", twrf::BoundingRegion::full_screen(), 5);
    graph.connect_dependency(root.id(), b1.id());
    b1.set_kernel([](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& s, const auto&) {
        int v = 20;
        s.write_output(self.id(), &v, sizeof(v), self.current_output_version() + 1, 1);
        return true;
    });

    auto& b2 = graph.add_twr(3, "BranchNode2", twrf::BoundingRegion::full_screen(), 5);
    graph.connect_dependency(root.id(), b2.id());
    b2.set_kernel([](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& s, const auto&) {
        int v = 30;
        s.write_output(self.id(), &v, sizeof(v), self.current_output_version() + 1, 1);
        return true;
    });

    auto& sink = graph.add_twr(4, "SinkNode", twrf::BoundingRegion::full_screen(), 10);
    graph.connect_dependency(b1.id(), sink.id());
    graph.connect_dependency(b2.id(), sink.id());
    sink.set_kernel([](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& s, const auto&) {
        int v = 100;
        s.write_output(self.id(), &v, sizeof(v), self.current_output_version() + 1, 1);
        return true;
    });

    TWRF_ASSERT(graph.validate_and_compute_depths(), "DAG validation failed");

    scheduler.run_frame(graph, state_store, out_trace, metrics);
}

int main() {
    TWRF_TEST_START("S1-07: Scheduler Determinism & Trace Repeatability");

    twrf::ExecutionTrace trace1;
    twrf::ExecutionTrace trace2;

    build_and_run_graph(trace1);
    build_and_run_graph(trace2);

    TWRF_ASSERT(trace1.entries().size() == trace2.entries().size(),
                "S1-07 Failed: Trace length differs across identical executions");
    TWRF_ASSERT(trace1.entries().size() == 4, "Expected 4 executions in trace");

    for (size_t i = 0; i < trace1.entries().size(); ++i) {
        const auto& e1 = trace1.entries()[i];
        const auto& e2 = trace2.entries()[i];

        TWRF_ASSERT(e1.step == e2.step, "Trace step mismatch");
        TWRF_ASSERT(e1.twr_id == e2.twr_id, "S1-07 Failed: TWR execution ordering is non-deterministic");
        TWRF_ASSERT(e1.executed == e2.executed, "Execution flag mismatch");
        TWRF_ASSERT(e1.reason == e2.reason, "Execution reason mismatch");
        TWRF_ASSERT(e1.output_version == e2.output_version, "Output version mismatch");
    }

    std::string str1 = trace1.to_string();
    std::string str2 = trace2.to_string();
    TWRF_ASSERT(str1 == str2, "S1-07 Failed: Formatted trace string mismatch");

    TWRF_TEST_PASS("S1-07: Scheduler Determinism & Trace Repeatability");
    return 0;
}
