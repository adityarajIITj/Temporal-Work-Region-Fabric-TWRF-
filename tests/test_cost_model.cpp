#include "twrf/core/cost_model.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S1-08: Corrected Break-Even Formula & Inverted Model Rejection");

    using CM = twrf::BreakEvenCostModel;

    const double Cr = 1000.0; // Recompute cost: 1000 cycles

    // Case 1: Low tracking overhead (Ct = 50 cycles, 5%)
    // Break-even threshold: p < 1 - 50/1000 = 0.95
    double Ct_low = 50.0;
    double max_p_low = CM::max_tolerable_change_rate(Ct_low, Cr);
    TWRF_ASSERT(max_p_low == 0.95, "S1-08 Failed: Max tolerable change should be 0.95");
    TWRF_ASSERT(CM::does_twrf_win(0.10, Ct_low, Cr), "TWRF must win at p = 0.10");
    TWRF_ASSERT(CM::does_twrf_win(0.50, Ct_low, Cr), "TWRF must win at p = 0.50");
    TWRF_ASSERT(CM::does_twrf_win(0.94, Ct_low, Cr), "TWRF must win at p = 0.94");
    TWRF_ASSERT(!CM::does_twrf_win(0.96, Ct_low, Cr), "TWRF must lose at p = 0.96");

    // Case 2: Moderate tracking overhead (Ct = 250 cycles, 25%)
    // Break-even threshold: p < 1 - 250/1000 = 0.75
    double Ct_med = 250.0;
    double max_p_med = CM::max_tolerable_change_rate(Ct_med, Cr);
    TWRF_ASSERT(max_p_med == 0.75, "S1-08 Failed: Max tolerable change should be 0.75");
    TWRF_ASSERT(CM::does_twrf_win(0.50, Ct_med, Cr), "TWRF must win at p = 0.50");
    TWRF_ASSERT(!CM::does_twrf_win(0.80, Ct_med, Cr), "TWRF must lose at p = 0.80");

    // Case 3: Extreme tracking overhead (Ct >= Cr, e.g. Ct = 1200)
    // TWRF can never win
    double Ct_high = 1200.0;
    TWRF_ASSERT(CM::max_tolerable_change_rate(Ct_high, Cr) == 0.0, "TWRF should tolerate 0 change when Ct >= Cr");
    TWRF_ASSERT(!CM::does_twrf_win(0.01, Ct_high, Cr), "TWRF cannot win even at 1% change when Ct > Cr");

    // Case 4: Monotonicity property
    // As Ct increases, maximum allowable change p strictly decreases
    TWRF_ASSERT(CM::max_tolerable_change_rate(100.0, Cr) > CM::max_tolerable_change_rate(300.0, Cr),
                "Monotonicity violation: higher Ct must lower break-even change threshold");

    // Case 5: Explicit rejection of the old/inverted formula (p > Ct / Cr)
    // Under old flawed formula: Ct = 200, Cr = 1000. It claimed TWRF wins when p > 0.20!
    // But at p = 0.90:
    // E[Full] = 1000
    // E[TWRF] = 200 + 0.90 * 1000 = 1100 (TWRF clearly LOSES!)
    // The old formula would erroneously predict TWRF wins because 0.90 > 0.20.
    TWRF_ASSERT(CM::is_old_inverted_formula_invalid(0.90, 200.0, Cr),
                "S1-08 Failed: Failed to catch and reject inverted formula error at p = 0.90");

    TWRF_TEST_PASS("S1-08: Corrected Break-Even Formula & Inverted Model Rejection");
    return 0;
}
