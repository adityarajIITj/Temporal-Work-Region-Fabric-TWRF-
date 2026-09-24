#include "twrf/timing/experiment_runner.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S4-01: Software Incremental Baseline C Execution Parity");

    const auto matrix =
        twrf::timing::ExperimentRunner::run_full_matrix(128, 16);

    TWRF_ASSERT(matrix.size() == 14,
                "S4-01: Full p x locality matrix must contain 14 cases");

    for (const auto& point : matrix) {
        TWRF_ASSERT(point.execution_set_parity,
                    "S4-01: B3 and TWRF execution sets must match");
        TWRF_ASSERT(point.output_parity,
                    "S4-01: B3 and TWRF outputs must be bitwise identical");
        TWRF_ASSERT(point.twrf_dependency_audit_failures == 0,
                    "S4-01: TWRF dependency audit reported a failure");
        TWRF_ASSERT(point.baseline_c_dependency_audit_failures == 0,
                    "S4-01: B3 dependency audit reported a failure");
    }

    TWRF_TEST_PASS("S4-01: Software Incremental Baseline C Execution Parity");
    return 0;
}
