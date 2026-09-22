#include "twrf/core/types.hpp"
#include "twrf/core/graph.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/state_store.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S1-10: Persistent Multi-Frame Execution without Graph Rebuild");

    twrf::TWRGraph graph;
    twrf::LogicalStateStore state_store;
    twrf::TWRScheduler scheduler;
    twrf::ExecutionTrace trace;
    twrf::MetricsCollector metrics;

    // Build a 4-node pipeline: InputA -> TWR1, InputB -> TWR2, (TWR1, TWR2) -> TWR3 -> TWR4
    auto& res_a = graph.add_resource(1, "InputA");
    auto& res_b = graph.add_resource(2, "InputB");
    res_a.set_value(10);
    res_b.set_value(20);

    auto& t1 = graph.add_twr(1, "TWR1");
    graph.bind_resource(t1.id(), res_a.id());
    t1.set_kernel([](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& s, const auto& in) {
        int v = in[0]->template get_value<int>(0) + 1;
        s.write_output(self.id(), &v, sizeof(v), self.current_output_version() + 1, 1);
        return true;
    });

    auto& t2 = graph.add_twr(2, "TWR2");
    graph.bind_resource(t2.id(), res_b.id());
    t2.set_kernel([](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& s, const auto& in) {
        int v = in[0]->template get_value<int>(0) * 3;
        s.write_output(self.id(), &v, sizeof(v), self.current_output_version() + 1, 1);
        return true;
    });

    auto& t3 = graph.add_twr(3, "TWR3");
    graph.connect_dependency(t1.id(), t3.id());
    graph.connect_dependency(t2.id(), t3.id());
    t3.set_kernel([](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& s, const auto&) {
        size_t sz;
        twrf::VersionNumber ver;
        int v1 = *reinterpret_cast<const int*>(s.read_output(1, sz, ver));
        int v2 = *reinterpret_cast<const int*>(s.read_output(2, sz, ver));
        int v = v1 + v2;
        s.write_output(self.id(), &v, sizeof(v), self.current_output_version() + 1, 1);
        return true;
    });

    auto& t4 = graph.add_twr(4, "TWR4");
    graph.connect_dependency(t3.id(), t4.id());
    t4.set_kernel([](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& s, const auto&) {
        size_t sz;
        twrf::VersionNumber ver;
        int v3 = *reinterpret_cast<const int*>(s.read_output(3, sz, ver));
        int v = v3 * 10;
        s.write_output(self.id(), &v, sizeof(v), self.current_output_version() + 1, 1);
        return true;
    });

    TWRF_ASSERT(graph.validate_and_compute_depths(), "DAG validation failed");

    // Execute 50 logical frames
    // Frame 0: All nodes execute (4 executions, 0 skips)
    // Even frames 2..48: Clean (0 executions, 4 skips each frame)
    // Frame 10: Mutate InputA only -> TWR1, TWR3, TWR4 execute (3 executions, 1 skip)
    // Frame 30: Mutate InputB only -> TWR2, TWR3, TWR4 execute (3 executions, 1 skip)

    const int TOTAL_FRAMES = 50;
    for (int f = 0; f < TOTAL_FRAMES; ++f) {
        if (f == 10) {
            res_a.set_value(15);
        } else if (f == 30) {
            res_b.set_value(25);
        }

        scheduler.run_frame(graph, state_store, trace, metrics);

        if (f == 0) {
            // Frame 0: Initial
            TWRF_ASSERT(t1.total_executions() == 1, "T1 exec Frame 0");
            TWRF_ASSERT(t2.total_executions() == 1, "T2 exec Frame 0");
            TWRF_ASSERT(t3.total_executions() == 1, "T3 exec Frame 0");
            TWRF_ASSERT(t4.total_executions() == 1, "T4 exec Frame 0");
        } else if (f == 10) {
            // T2 should have skipped; T1, T3, T4 executed
            TWRF_ASSERT(t1.total_executions() == 2, "T1 should execute on frame 10");
            TWRF_ASSERT(t2.total_executions() == 1, "T2 must remain skipped on frame 10");
            TWRF_ASSERT(t3.total_executions() == 2, "T3 should execute on frame 10");
            TWRF_ASSERT(t4.total_executions() == 2, "T4 should execute on frame 10");
        } else if (f == 30) {
            // T1 should have skipped; T2, T3, T4 executed
            TWRF_ASSERT(t1.total_executions() == 2, "T1 must remain skipped on frame 30");
            TWRF_ASSERT(t2.total_executions() == 2, "T2 should execute on frame 30");
            TWRF_ASSERT(t3.total_executions() == 3, "T3 should execute on frame 30");
            TWRF_ASSERT(t4.total_executions() == 3, "T4 should execute on frame 30");
        }
    }

    TWRF_ASSERT(metrics.frames_executed == TOTAL_FRAMES, "Frames count mismatch");
    TWRF_ASSERT(metrics.skip_ratio() > 0.80, "Multi-frame skip ratio should exceed 80% on mostly static scenes");

    TWRF_TEST_PASS("S1-10: Persistent Multi-Frame Execution without Graph Rebuild");
    return 0;
}
