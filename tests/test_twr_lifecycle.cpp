#include "twrf/core/types.hpp"
#include "twrf/core/graph.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/state_store.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S1-01: Allocate, Execute, and Reuse One TWR");

    twrf::TWRGraph graph;
    twrf::LogicalStateStore state_store;
    twrf::TWRScheduler scheduler;
    twrf::ExecutionTrace trace;
    twrf::MetricsCollector metrics;

    // 1. Create input resource
    auto& res = graph.add_resource(1, "InputBuffer");
    int initial_data = 42;
    res.set_value(initial_data);

    // 2. Create TWR with kernel counter
    int kernel_exec_count = 0;
    auto& twr = graph.add_twr(100, "ComputeTWR");
    graph.bind_resource(twr.id(), res.id());

    twr.set_kernel([&](twrf::TemporalWorkRegion& self,
                       twrf::LogicalStateStore& store,
                       const std::vector<const twrf::VersionedResource*>& inputs) -> bool {
        kernel_exec_count++;
        int val = inputs[0]->get_value<int>(0);
        int result = val * 2;
        store.write_output(self.id(), &result, sizeof(result), self.current_output_version() + 1, 1);
        return true;
    });

    TWRF_ASSERT(graph.validate_and_compute_depths(), "Graph validation failed");

    // --- Frame 1: Initial execution ---
    scheduler.run_frame(graph, state_store, trace, metrics);

    TWRF_ASSERT(kernel_exec_count == 1, "Kernel should execute exactly once in Frame 1");
    TWRF_ASSERT(metrics.total_twr_executions == 1, "Metrics execution count should be 1");
    TWRF_ASSERT(metrics.total_twr_skips == 0, "Metrics skip count should be 0");

    size_t out_size = 0;
    twrf::VersionNumber out_ver = 0;
    const uint8_t* out_data = state_store.read_output(twr.id(), out_size, out_ver);
    TWRF_ASSERT(out_data != nullptr && out_size == sizeof(int), "Output not written to State Store");
    TWRF_ASSERT(*reinterpret_cast<const int*>(out_data) == 84, "Computed output incorrect");

    // --- Frame 2: Identical inputs (No mutation) ---
    scheduler.run_frame(graph, state_store, trace, metrics);

    TWRF_ASSERT(kernel_exec_count == 1, "Kernel execution count MUST NOT increment on clean frame (reused output)");
    TWRF_ASSERT(metrics.total_twr_executions == 1, "Total executions must remain 1");
    TWRF_ASSERT(metrics.total_twr_skips == 1, "Total skips must increment to 1");

    // Verify output state in State Store is still accessible and unchanged
    out_data = state_store.read_output(twr.id(), out_size, out_ver);
    TWRF_ASSERT(*reinterpret_cast<const int*>(out_data) == 84, "Reused output value modified unexpectedly");

    TWRF_TEST_PASS("S1-01: Allocate, Execute, and Reuse One TWR");
    return 0;
}
