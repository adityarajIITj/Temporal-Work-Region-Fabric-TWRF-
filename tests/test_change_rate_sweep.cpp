#include "twrf/timing/experiment_runner.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S3-03: Change-Rate Sweeps Across All 7 Regimes (0% to 100%)");

    auto change_data = twrf::timing::ExperimentRunner::run_change_rate_sweep(128, 16);
    auto locality_data = twrf::timing::ExperimentRunner::run_locality_sweep(128, 16);

    // Invariant 1: Exactly 7 change rate points executed
    TWRF_ASSERT(change_data.size() == 7, "S3-03 Failed: Must execute all 7 change rates");

    double expected_rates[] = {0.00, 0.05, 0.10, 0.25, 0.50, 0.75, 1.00};
    for (size_t i = 0; i < change_data.size(); ++i) {
        TWRF_ASSERT(std::abs(change_data[i].change_rate - expected_rates[i]) < 1e-6,
                    "S3-03 Failed: Change rate point mismatch");
        TWRF_ASSERT(change_data[i].twrf_cycles.is_strictly_accounted(),
                    "S3-03 Failed: Unaccounted TWRF cycles in change rate sweep");
        TWRF_ASSERT(change_data[i].baseline_c_cycles.is_strictly_accounted(),
                    "S3-03 Failed: Unaccounted B3 cycles in change rate sweep");
        TWRF_ASSERT(change_data[i].execution_set_parity,
                    "S3-03 Failed: B3 execution-set parity violated");
        TWRF_ASSERT(change_data[i].output_parity,
                    "S3-03 Failed: B3 output parity violated");
        TWRF_ASSERT(change_data[i].twrf_dependency_audit_failures == 0,
                    "S3-03 Failed: TWRF dependency audit failure");
        TWRF_ASSERT(change_data[i].baseline_c_dependency_audit_failures == 0,
                    "S3-03 Failed: B3 dependency audit failure");
    }

    // Invariant 2: At p = 0.00, TWRF strictly wins over Baseline A
    TWRF_ASSERT(change_data[0].twrf_wins_vs_a(),
                "S3-03 Failed: TWRF must strictly outperform Baseline A at 0% change");

    // Export machine-readable JSON results
    bool ok = twrf::timing::ExperimentRunner::export_json("results/phase3_sweeps.json", change_data, locality_data);
    TWRF_ASSERT(ok, "S3-03 Failed: Failed to export machine-readable JSON results to results/phase3_sweeps.json");

    TWRF_TEST_PASS("S3-03: Change-Rate Sweeps + B3 Parity Across 7 Regimes (0% to 100%)");
    return 0;
}
