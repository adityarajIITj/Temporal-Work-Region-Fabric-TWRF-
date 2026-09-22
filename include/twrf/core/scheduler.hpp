#pragma once

#include "twrf/core/types.hpp"
#include "twrf/core/twr.hpp"
#include "twrf/core/graph.hpp"
#include "twrf/core/state_store.hpp"
#include "twrf/core/change_tracker.hpp"
#include "twrf/core/trace.hpp"
#include "twrf/core/metrics.hpp"
#include <queue>
#include <vector>
#include <tuple>
#include <unordered_set>

namespace twrf {

struct ReadyQueueItem {
    int32_t priority{0};
    uint32_t depth{0};
    TWRId twr_id{0};

    // Strict deterministic tie-breaking:
    // 1. Higher priority first
    // 2. Deeper topological depth first
    // 3. Lower TWRId first
    bool operator<(const ReadyQueueItem& other) const noexcept {
        if (priority != other.priority) {
            return priority < other.priority; // higher priority popped first
        }
        if (depth != other.depth) {
            return depth < other.depth; // deeper depth popped first
        }
        return twr_id > other.twr_id; // smaller ID popped first
    }
};

class TWRScheduler {
public:
    TWRScheduler() = default;

    [[nodiscard]] size_t ready_queue_size() const noexcept {
        return ready_queue_.size();
    }

    [[nodiscard]] Timestamp current_step() const noexcept {
        return step_counter_;
    }

    void prepare_frame(TWRGraph& graph, LogicalStateStore& state_store,
                       ExecutionTrace& trace, MetricsCollector& metrics) {
        metrics.frames_executed++;

        // 1. Evaluate change detection for all TWRs against external resources
        for (const auto& [id, twr] : graph.twrs()) {
            metrics.dirty_evaluations++;
            ChangeTracker::evaluate_dirty(*twr, graph, state_store);
        }

        // 2. Propagate dirty state downstream across the DAG
        std::queue<TWRId> dirty_queue;
        for (const auto& [id, twr] : graph.twrs()) {
            if (twr->status() == TWRStatus::Dirty) {
                dirty_queue.push(id);
            }
        }

        while (!dirty_queue.empty()) {
            TWRId curr_id = dirty_queue.front();
            dirty_queue.pop();
            const auto* curr = graph.get_twr(curr_id);
            if (!curr) continue;

            for (TWRId consumer_id : curr->downstream_consumers()) {
                auto* consumer = graph.get_twr(consumer_id);
                if (consumer && consumer->status() != TWRStatus::Dirty) {
                    consumer->mark_dirty(ExecutionReason::ProducerOutputChanged);
                    dirty_queue.push(consumer_id);
                }
            }
        }

        // 3. For clean TWRs: record skip
        //    For dirty TWRs: count only upstream producers that are also scheduled to execute (will_execute)
        //    If active dirty producers == 0, push immediately to ready queue
        std::unordered_set<TWRId> will_execute;
        for (const auto& [id, twr] : graph.twrs()) {
            if (twr->status() == TWRStatus::Dirty) {
                will_execute.insert(id);
            }
        }

        for (const auto& [id, twr] : graph.twrs()) {
            if (!will_execute.count(id)) {
                std::vector<std::pair<ResourceId, VersionNumber>> in_vers;
                for (const auto& b : twr->resource_bindings()) {
                    const auto* res = graph.get_resource(b.resource_id);
                    in_vers.emplace_back(b.resource_id, res ? res->version() : INVALID_VERSION);
                }
                trace.record_skip(step_counter_, twr->id(), twr->name(), in_vers);
                twr->record_skip();
                metrics.total_twr_skips++;
            } else {
                uint32_t active_dirty_producers = 0;
                for (const auto& up : twr->upstream_producers()) {
                    if (will_execute.count(up.producer_id)) {
                        active_dirty_producers++;
                    }
                }
                twr->set_pending_dependencies(active_dirty_producers);
                if (active_dirty_producers == 0) {
                    enqueue_ready(*twr);
                }
            }
        }
    }

    bool step(TWRGraph& graph, LogicalStateStore& state_store,
              ExecutionTrace& trace, MetricsCollector& metrics) {
        if (ready_queue_.empty()) return false;

        ReadyQueueItem item = ready_queue_.top();
        ready_queue_.pop();

        auto* twr = graph.get_twr(item.twr_id);
        if (!twr) return false;

        step_counter_++;

        // Gather input resources
        std::vector<const VersionedResource*> inputs;
        std::vector<std::pair<ResourceId, VersionNumber>> in_vers;
        for (const auto& binding : twr->resource_bindings()) {
            const auto* res = graph.get_resource(binding.resource_id);
            inputs.push_back(res);
            in_vers.emplace_back(binding.resource_id, res ? res->version() : INVALID_VERSION);
        }

        ExecutionReason reason = twr->execution_reason();

        // Execute kernel
        bool success = twr->execute(state_store, inputs, step_counter_);
        if (success) {
            metrics.total_twr_executions++;
            trace.record_execution(step_counter_, twr->id(), twr->name(),
                                   reason, twr->current_output_version(), in_vers);

            // Notify downstream consumers
            for (TWRId consumer_id : twr->downstream_consumers()) {
                metrics.dependency_traversals++;
                auto* consumer = graph.get_twr(consumer_id);
                if (!consumer) continue;

                consumer->decrement_pending_dependencies();
                if (consumer->pending_dependencies() == 0 && consumer->status() == TWRStatus::Dirty) {
                    enqueue_ready(*consumer);
                }
            }
        }

        return true;
    }

    void run_frame(TWRGraph& graph, LogicalStateStore& state_store,
                   ExecutionTrace& trace, MetricsCollector& metrics) {
        prepare_frame(graph, state_store, trace, metrics);
        while (step(graph, state_store, trace, metrics)) {
            // Drain ready queue
        }
    }

    void reset() noexcept {
        while (!ready_queue_.empty()) ready_queue_.pop();
        step_counter_ = 0;
    }

private:
    void enqueue_ready(TemporalWorkRegion& twr) {
        twr.set_ready();
        ready_queue_.push(ReadyQueueItem{
            twr.priority(),
            twr.topological_depth(),
            twr.id()
        });
    }

    std::priority_queue<ReadyQueueItem> ready_queue_;
    Timestamp step_counter_{0};
};

} // namespace twrf
