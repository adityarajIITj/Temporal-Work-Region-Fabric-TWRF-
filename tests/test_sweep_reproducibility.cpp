#include "twrf/timing/experiment_runner.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S3-06: Scientific Sweep Reproducibility Across Reruns");

    // Run sweep twice independently
    auto run1 = twrf::timing::ExperimentRunner::run_change_rate_sweep(128, 16);
    auto run2 = twrf::timing::ExperimentRunner::run_change_rate_sweep(128, 16);

    TWRF_ASSERT(run1.size() == run2.size(), "Sweep run lengths differ");

    for (size_t i = 0; i < run1.size(); ++i) {
        const auto& p1 = run1[i];
        const auto& p2 = run2[i];

        TWRF_ASSERT(p1.change_rate == p2.change_rate, "Change rate mismatch");
        TWRF_ASSERT(p1.tiles_executed == p2.tiles_executed, "Tiles executed count differs across reruns");
        TWRF_ASSERT(p1.tiles_skipped == p2.tiles_skipped, "Tiles skipped count differs across reruns");
        TWRF_ASSERT(std::abs(p1.twrf_cycles.total_cycles() - p2.twrf_cycles.total_cycles()) < 1e-6,
                    "S3-06 Failed: TWRF total cycles non-deterministic across reruns");
        TWRF_ASSERT(std::abs(p1.baseline_a_cycles.total_cycles() - p2.baseline_a_cycles.total_cycles()) < 1e-6,
                    "S3-06 Failed: Baseline A total cycles non-deterministic across reruns");
        TWRF_ASSERT(std::abs(p1.baseline_b_cycles.total_cycles() - p2.baseline_b_cycles.total_cycles()) < 1e-6,
                    "S3-06 Failed: Baseline B total cycles non-deterministic across reruns");
        TWRF_ASSERT(std::abs(p1.baseline_c_cycles.total_cycles() - p2.baseline_c_cycles.total_cycles()) < 1e-6,
                    "S3-06 Failed: Baseline C total cycles non-deterministic across reruns");
        TWRF_ASSERT(p1.execution_set_parity == p2.execution_set_parity && p1.execution_set_parity,
                    "S3-06 Failed: B3 execution parity is non-deterministic");
        TWRF_ASSERT(p1.output_parity == p2.output_parity && p1.output_parity,
                    "S3-06 Failed: B3 output parity is non-deterministic");
    }

    TWRF_TEST_PASS("S3-06: Scientific Sweep Reproducibility Across Reruns");
    return 0;
}
