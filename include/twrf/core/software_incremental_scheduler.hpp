#pragma once

#include "twrf/core/types.hpp"
#include "twrf/core/twr.hpp"
#include "twrf/core/graph.hpp"
#include "twrf/core/state_store.hpp"
#include "twrf/core/trace.hpp"
#include "twrf/core/metrics.hpp"
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <vector>

namespace twrf {

/**
 * Software-equivalent incremental scheduler used as Baseline C.
 *
 * This runtime intentionally avoids the hardware-oriented ready priority
 * queue used by TWRScheduler. It performs explicit software scans and a
 * linear ready-set selection while preserving the same TWR semantics,
 * dependency graph, mutation events, and kernel execution functions.
 *
 * It is a semantic baseline, not a timing simulator. Its operation counts
 * are captured through MetricsCollector and converted to modeled cycles by
 * the timing layer.
 */
class SoftwareIncrementalScheduler {
public:
    [[nodiscard]] size_t ready_set_size() const noexcept {
        return ready_set_.size();
    }

    [[nodiscard]] Timestamp current_step() const noexcept {
        return step_counter_;
    }

    void prepare_frame(TWRGraph& graph, LogicalStateStore& state_store,
                       ExecutionTrace& trace, MetricsCollector& metrics) {
        metrics.frames_executed++;

        // Software explicitly scans every TWR and every bound resource.
        for (const auto& [id, twr] : graph.twrs()) {
            metrics.dirty_evaluations++;
            if (twr->status() == TWRStatus::Dirty) {
                continue;
            }

            for (const auto& binding : twr->resource_bindings()) {
                const auto* res = graph.get_resource(binding.resource_id);
                if (!res) continue;
                if (res->version() != binding.recorded_version &&
                    twr->region().overlaps(res->bounds())) {
                    twr->mark_dirty(ExecutionReason::InputVersionChanged);
                    break;
                }
            }

            if (twr->status() != TWRStatus::Dirty) {
                for (const auto& up : twr->upstream_producers()) {
                    const VersionNumber producer_version =
                        state_store.get_output_version(up.producer_id);
                    if (producer_version != up.recorded_version) {
                        twr->mark_dirty(ExecutionReason::ProducerOutputChanged);
                        break;
                    }
                }
            }
        }

        // Explicit software dirty propagation through the DAG.
        std::queue<TWRId> dirty_queue;
        for (const auto& [id, twr] : graph.twrs()) {
            if (twr->status() == TWRStatus::Dirty) {
                dirty_queue.push(id);
            }
        }

        while (!dirty_queue.empty()) {
            const TWRId current = dirty_queue.front();
            dirty_queue.pop();

            auto* producer = graph.get_twr(current);
            if (!producer) continue;

            for (const TWRId consumer_id : producer->downstream_consumers()) {
                auto* consumer = graph.get_twr(consumer_id);
                if (!consumer || consumer->status() == TWRStatus::Dirty) continue;
                consumer->mark_dirty(ExecutionReason::ProducerOutputChanged);
                dirty_queue.push(consumer_id);
            }
        }

        // Record clean regions as skips and seed the software ready set.
        for (const auto& [id, twr] : graph.twrs()) {
            if (twr->status() != TWRStatus::Dirty) {
                std::vector<std::pair<ResourceId, VersionNumber>> input_versions;
                for (const auto& binding : twr->resource_bindings()) {
                    const auto* resource = graph.get_resource(binding.resource_id);
                    input_versions.emplace_back(
                        binding.resource_id,
                        resource ? resource->version() : INVALID_VERSION);
                }
                trace.record_skip(step_counter_, twr->id(), twr->name(),
                                  input_versions);
                twr->record_skip();
                metrics.total_twr_skips++;
                continue;
            }

            uint32_t pending = 0;
            for (const auto& up : twr->upstream_producers()) {
                const auto* producer = graph.get_twr(up.producer_id);
                if (producer && producer->status() == TWRStatus::Dirty) {
                    pending++;
                }
            }
            twr->set_pending_dependencies(pending);

            if (pending == 0) {
                ready_set_.insert(twr->id());
            }
        }
    }

    bool step(TWRGraph& graph, LogicalStateStore& state_store,
              ExecutionTrace& trace, MetricsCollector& metrics) {
        if (ready_set_.empty()) return false;

        const TWRId selected = select_next_ready(graph);
        ready_set_.erase(selected);

        auto* twr = graph.get_twr(selected);
        if (!twr) return false;

        step_counter_++;

        std::vector<const VersionedResource*> inputs;
        std::vector<std::pair<ResourceId, VersionNumber>> input_versions;
        for (const auto& binding : twr->resource_bindings()) {
            const auto* resource = graph.get_resource(binding.resource_id);
            inputs.push_back(resource);
            input_versions.emplace_back(
                binding.resource_id,
                resource ? resource->version() : INVALID_VERSION);
        }

        const ExecutionReason reason = twr->execution_reason();
        const bool success = twr->execute(state_store, inputs, step_counter_);

        if (twr->dependency_audit_enabled()) {
            // Only completed executions are meaningful audit observations.
            if (twr->dependency_audit_was_evaluated()) {
                if (twr->dependency_audit_passed_last_execution()) {
                    metrics.dependency_audit_passes++;
                } else {
                    metrics.dependency_audit_failures++;
                }
            }
        }

        if (success) {
            metrics.total_twr_executions++;
            trace.record_execution(step_counter_, twr->id(), twr->name(),
                                   reason, twr->current_output_version(),
                                   input_versions);

            for (const TWRId consumer_id : twr->downstream_consumers()) {
                metrics.dependency_traversals++;
                auto* consumer = graph.get_twr(consumer_id);
                if (!consumer) continue;

                consumer->decrement_pending_dependencies();
                if (consumer->pending_dependencies() == 0 &&
                    consumer->status() == TWRStatus::Dirty) {
                    ready_set_.insert(consumer_id);
                }
            }
        } else {
            twr->mark_dirty(ExecutionReason::None);
        }

        return true;
    }

    void run_frame(TWRGraph& graph, LogicalStateStore& state_store,
                   ExecutionTrace& trace, MetricsCollector& metrics) {
        prepare_frame(graph, state_store, trace, metrics);
        while (step(graph, state_store, trace, metrics)) {
        }
    }

    void reset() noexcept {
        ready_set_.clear();
        step_counter_ = 0;
    }

private:
    [[nodiscard]] TWRId select_next_ready(const TWRGraph& graph) const {
        auto best = ready_set_.begin();
        for (auto it = std::next(ready_set_.begin()); it != ready_set_.end(); ++it) {
            const auto* lhs = graph.get_twr(*best);
            const auto* rhs = graph.get_twr(*it);
            if (!lhs || !rhs) continue;

            // Match the architectural scheduler's deterministic semantic
            // ordering, but implement it as an explicit software scan.
            if (rhs->priority() > lhs->priority() ||
                (rhs->priority() == lhs->priority() &&
                 (rhs->topological_depth() > lhs->topological_depth() ||
                  (rhs->topological_depth() == lhs->topological_depth() &&
                   rhs->id() < lhs->id())))) {
                best = it;
            }
        }
        return *best;
    }

    std::unordered_set<TWRId> ready_set_;
    Timestamp step_counter_{0};
};

} // namespace twrf
