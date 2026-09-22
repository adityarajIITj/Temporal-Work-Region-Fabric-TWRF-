#pragma once

#include <cstdint>
#include <stdexcept>
#include <algorithm>

namespace twrf {

class BreakEvenCostModel {
public:
    // Expected recomputation cost: E[Full] = C_r
    static constexpr double expected_full_cost(double Cr) noexcept {
        return Cr;
    }

    // Expected TWRF cost: E[TWRF] = C_t + p * C_r
    static constexpr double expected_twrf_cost(double p, double Ct, double Cr) noexcept {
        return Ct + (p * Cr);
    }

    // True mathematical break-even condition:
    // TWRF wins when C_t + p * C_r < C_r <=> p < 1 - (C_t / C_r)
    static constexpr bool does_twrf_win(double p, double Ct, double Cr) {
        if (Cr <= 0.0) {
            throw std::invalid_argument("Recompute cost Cr must be strictly positive");
        }
        if (Ct < 0.0 || p < 0.0 || p > 1.0) {
            throw std::invalid_argument("Invalid cost or probability parameters");
        }
        return expected_twrf_cost(p, Ct, Cr) < expected_full_cost(Cr);
    }

    // Maximum tolerable change rate before TWRF becomes a net loss
    static constexpr double max_tolerable_change_rate(double Ct, double Cr) {
        if (Cr <= 0.0) return 0.0;
        double ratio = Ct / Cr;
        return (ratio < 1.0) ? (1.0 - ratio) : 0.0;
    }

    // Explicit rejection helper for test S1-08:
    // Confirms that the old inverted formula (p > Ct / Cr) is false for high change rates
    static constexpr bool is_old_inverted_formula_invalid(double p, double Ct, double Cr) {
        // Under old formula: if p > Ct/Cr, claim TWRF wins.
        // But if p is close to 1.0 and Ct > 0, TWRF actually LOSES.
        bool old_claim = (p > (Ct / Cr));
        bool actual_win = does_twrf_win(p, Ct, Cr);
        // The old formula is deemed invalid whenever its prediction contradicts physical reality
        return old_claim != actual_win;
    }
};

} // namespace twrf
