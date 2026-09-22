#pragma once

#include "twrf/core/types.hpp"
#include <cstdint>

namespace twrf {

struct MetricsCollector {
    uint64_t frames_executed{0};
    uint64_t total_twr_executions{0};
    uint64_t total_twr_skips{0};
    uint64_t dependency_traversals{0};
    uint64_t dirty_evaluations{0};

    [[nodiscard]] double skip_ratio() const noexcept {
        uint64_t total = total_twr_executions + total_twr_skips;
        return total > 0 ? static_cast<double>(total_twr_skips) / static_cast<double>(total) : 0.0;
    }

    void reset() noexcept {
        frames_executed = 0;
        total_twr_executions = 0;
        total_twr_skips = 0;
        dependency_traversals = 0;
        dirty_evaluations = 0;
    }
};

} // namespace twrf
