#pragma once

#include <cstdint>
#include <string>

namespace twrf::timing {

struct TimingParameters {
    // Computation parameters (Recompute work Cr)
    double cycles_per_triangle{40.0};
    double cycles_per_fragment{2.0};

    // Tracking parameters (Ct components)
    double cycles_version_check{5.0};        // C_change_detect: scalar comparison
    double cycles_bounding_check{10.0};      // C_change_detect: bounding box intersection
    double cycles_queue_operation{15.0};     // C_schedule: ready queue push/pop
    double cycles_dependency_notify{8.0};    // C_dependency: consumer counter decrement

    // Baseline C: software incremental runtime management.
    // These parameters model explicit software bookkeeping rather than a
    // hardware TWRF scoreboard/ready queue.
    double sw_cycles_version_check{12.0};
    double sw_cycles_queue_operation{35.0};
    double sw_cycles_dependency_notify{18.0};
    double sw_cycles_state_store_read_byte{0.12};
    double sw_cycles_state_store_write_byte{0.12};

    // Memory hierarchy parameters
    double cycles_state_store_read_byte{0.05};   // On-chip SRAM read cost
    double cycles_state_store_write_byte{0.05};  // On-chip SRAM write cost
    double cycles_scene_dram_read_byte{1.50};    // Off-chip DRAM/HBM access cost

    // Logical NoC interconnect
    double cycles_noc_hop{3.0};                  // Per Manhattan distance unit

    // Baseline B (Conventional Temporal Cache) parameters
    double cycles_cache_tag_lookup{25.0};       // Software/hardware cache lookup
    double cycles_cache_miss_penalty{120.0};     // Cache refill and validation
    double cycles_cache_eviction{40.0};          // Cache writeback

    static TimingParameters default_config() noexcept {
        return TimingParameters{};
    }

    static TimingParameters high_tracking_overhead() noexcept {
        TimingParameters p;
        p.cycles_version_check = 50.0;
        p.cycles_bounding_check = 100.0;
        p.cycles_queue_operation = 150.0;
        p.cycles_dependency_notify = 80.0;
        return p;
    }
};

} // namespace twrf::timing
