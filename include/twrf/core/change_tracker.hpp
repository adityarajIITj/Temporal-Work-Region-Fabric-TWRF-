#pragma once

#include "twrf/core/types.hpp"
#include "twrf/core/twr.hpp"
#include "twrf/core/graph.hpp"
#include "twrf/core/state_store.hpp"

namespace twrf {

class ChangeTracker {
public:
    static bool evaluate_dirty(TemporalWorkRegion& twr, const TWRGraph& graph, const LogicalStateStore& state_store) {
        if (twr.status() == TWRStatus::Dirty) {
            return true; // Already dirty
        }

        // 1. External resource input changes
        for (const auto& binding : twr.resource_bindings()) {
            const auto* res = graph.get_resource(binding.resource_id);
            if (!res) continue;

            if (res->version() != binding.recorded_version) {
                // Check conservative spatial bounding filter
                if (twr.region().overlaps(res->bounds())) {
                    twr.mark_dirty(ExecutionReason::InputVersionChanged);
                    return true;
                }
            }
        }

        // 2. Upstream producer output changes
        for (const auto& up : twr.upstream_producers()) {
            VersionNumber cur_prod_ver = state_store.get_output_version(up.producer_id);
            if (cur_prod_ver != up.recorded_version) {
                twr.mark_dirty(ExecutionReason::ProducerOutputChanged);
                return true;
            }
        }

        return false;
    }

    static bool apply_conservative_invalidation(TemporalWorkRegion& twr, const BoundingRegion& dirty_bounds) {
        if (twr.region().overlaps(dirty_bounds)) {
            twr.mark_dirty(ExecutionReason::ConservativeInvalidation);
            return true;
        }
        return false;
    }
};

} // namespace twrf
