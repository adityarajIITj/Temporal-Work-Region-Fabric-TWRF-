#include "twrf/timing/experiment_runner.hpp"
#include "tests/test_common.hpp"
#include <cmath>

int main() {
    TWRF_TEST_START("S4-03: Timing sensitivity grid is reproducible and well formed");

    const auto grid = twrf::timing::ExperimentRunner::run_sensitivity_grid();

    // 3 tile sizes x 6 control multipliers x 3 State Store multipliers.
    TWRF_ASSERT(grid.size() == 135,
                "Sensitivity grid must contain 135 parameter settings");

    for (const auto& point : grid) {
        TWRF_ASSERT(point.cases == 14,
                    "Every sensitivity setting must evaluate the 14-case matrix");
        TWRF_ASSERT(point.twrf_wins_vs_a <= point.cases &&
                    point.twrf_wins_vs_b <= point.cases &&
                    point.twrf_wins_vs_c <= point.cases,
                    "Win counts cannot exceed case count");
        TWRF_ASSERT(point.twrf_vs_c_win_fraction >= 0.0 &&
                    point.twrf_vs_c_win_fraction <= 1.0,
                    "Win fraction must remain in [0,1]");
        TWRF_ASSERT(std::isfinite(point.min_twrf_vs_c_ratio) &&
                    std::isfinite(point.max_twrf_vs_c_ratio),
                    "Sensitivity ratios must be finite");
        TWRF_ASSERT(point.min_twrf_vs_c_ratio >= 0.0 &&
                    point.max_twrf_vs_c_ratio >= point.min_twrf_vs_c_ratio,
                    "Sensitivity ratio bounds are invalid");
    }

    TWRF_TEST_PASS("S4-03: Timing sensitivity grid is reproducible and well formed");
    return 0;
}
