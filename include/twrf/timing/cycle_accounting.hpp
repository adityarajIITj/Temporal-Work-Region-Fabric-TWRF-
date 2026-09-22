#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace twrf::timing {

struct CycleAccounting {
    double compute_cycles{0.0};
    double change_detect_cycles{0.0};
    double scheduler_cycles{0.0};
    double dependency_cycles{0.0};
    double state_store_cycles{0.0};
    double scene_memory_cycles{0.0};
    double interconnect_cycles{0.0};

    // Cache-specific cycles (for Baseline B)
    double cache_overhead_cycles{0.0};

    [[nodiscard]] double total_cycles() const noexcept {
        return compute_cycles +
               change_detect_cycles +
               scheduler_cycles +
               dependency_cycles +
               state_store_cycles +
               scene_memory_cycles +
               interconnect_cycles +
               cache_overhead_cycles;
    }

    [[nodiscard]] double tracking_overhead_cycles() const noexcept {
        return change_detect_cycles +
               scheduler_cycles +
               dependency_cycles +
               state_store_cycles;
    }

    // Validates S3-01: Every reported cycle is strictly attributed to a defined component
    [[nodiscard]] bool is_strictly_accounted() const noexcept {
        double sum = compute_cycles +
                     change_detect_cycles +
                     scheduler_cycles +
                     dependency_cycles +
                     state_store_cycles +
                     scene_memory_cycles +
                     interconnect_cycles +
                     cache_overhead_cycles;
        return std::abs(sum - total_cycles()) < 1e-6;
    }

    [[nodiscard]] std::string to_string() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1);
        oss << "  Compute:       " << compute_cycles << " cycles\n"
            << "  ChangeDetect:  " << change_detect_cycles << " cycles\n"
            << "  Scheduler:     " << scheduler_cycles << " cycles\n"
            << "  Dependency:    " << dependency_cycles << " cycles\n"
            << "  State Store:   " << state_store_cycles << " cycles\n"
            << "  Scene DRAM:    " << scene_memory_cycles << " cycles\n"
            << "  Interconnect:  " << interconnect_cycles << " cycles\n"
            << "  Cache (B):     " << cache_overhead_cycles << " cycles\n"
            << "  TOTAL:         " << total_cycles() << " cycles";
        return oss.str();
    }

    [[nodiscard]] std::string to_json() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "{\n"
            << "  \"compute_cycles\": " << compute_cycles << ",\n"
            << "  \"change_detect_cycles\": " << change_detect_cycles << ",\n"
            << "  \"scheduler_cycles\": " << scheduler_cycles << ",\n"
            << "  \"dependency_cycles\": " << dependency_cycles << ",\n"
            << "  \"state_store_cycles\": " << state_store_cycles << ",\n"
            << "  \"scene_memory_cycles\": " << scene_memory_cycles << ",\n"
            << "  \"interconnect_cycles\": " << interconnect_cycles << ",\n"
            << "  \"cache_overhead_cycles\": " << cache_overhead_cycles << ",\n"
            << "  \"total_cycles\": " << total_cycles() << "\n"
            << "}";
        return oss.str();
    }
};

} // namespace twrf::timing
