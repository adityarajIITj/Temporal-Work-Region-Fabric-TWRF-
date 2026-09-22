#pragma once

#include "twrf/core/types.hpp"
#include "twrf/core/resource.hpp"
#include "twrf/core/twr.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include <queue>

namespace twrf {

class TWRGraph {
public:
    TWRGraph() = default;

    VersionedResource& add_resource(ResourceId id, std::string name, BoundingRegion bounds = BoundingRegion::full_screen()) {
        auto res = std::make_unique<VersionedResource>(id, std::move(name), bounds);
        auto* ptr = res.get();
        resources_[id] = std::move(res);
        return *ptr;
    }

    TemporalWorkRegion& add_twr(TWRId id, std::string name, BoundingRegion region = BoundingRegion::full_screen(), int32_t priority = 0) {
        auto twr = std::make_unique<TemporalWorkRegion>(id, std::move(name), region, priority);
        auto* ptr = twr.get();
        twrs_[id] = std::move(twr);
        return *ptr;
    }

    void connect_dependency(TWRId producer_id, TWRId consumer_id) {
        auto* producer = get_twr(producer_id);
        auto* consumer = get_twr(consumer_id);
        if (!producer || !consumer) {
            throw std::runtime_error("Invalid TWR IDs in connect_dependency");
        }
        producer->add_downstream_consumer(consumer_id);
        consumer->add_upstream_dependency(producer_id);
    }

    void bind_resource(TWRId twr_id, ResourceId resource_id) {
        auto* twr = get_twr(twr_id);
        auto* res = get_resource(resource_id);
        if (!twr || !res) {
            throw std::runtime_error("Invalid ID in bind_resource");
        }
        twr->bind_resource(resource_id);
    }

    [[nodiscard]] VersionedResource* get_resource(ResourceId id) const noexcept {
        auto it = resources_.find(id);
        return (it != resources_.end()) ? it->second.get() : nullptr;
    }

    [[nodiscard]] TemporalWorkRegion* get_twr(TWRId id) const noexcept {
        auto it = twrs_.find(id);
        return (it != twrs_.end()) ? it->second.get() : nullptr;
    }

    [[nodiscard]] const std::unordered_map<ResourceId, std::unique_ptr<VersionedResource>>& resources() const noexcept {
        return resources_;
    }

    [[nodiscard]] const std::unordered_map<TWRId, std::unique_ptr<TemporalWorkRegion>>& twrs() const noexcept {
        return twrs_;
    }

    // Topological sort and depth computation with cycle detection
    bool validate_and_compute_depths() {
        std::unordered_map<TWRId, uint32_t> in_degree;
        for (const auto& [id, twr] : twrs_) {
            in_degree[id] = static_cast<uint32_t>(twr->upstream_producers().size());
        }

        std::queue<TWRId> q;
        for (const auto& [id, deg] : in_degree) {
            if (deg == 0) {
                q.push(id);
                twrs_[id]->set_topological_depth(0);
            }
        }

        size_t visited_count = 0;
        while (!q.empty()) {
            TWRId curr_id = q.front();
            q.pop();
            visited_count++;

            auto* curr = twrs_[curr_id].get();
            uint32_t next_depth = curr->topological_depth() + 1;

            for (TWRId child_id : curr->downstream_consumers()) {
                auto* child = twrs_[child_id].get();
                if (child->topological_depth() < next_depth) {
                    child->set_topological_depth(next_depth);
                }
                if (--in_degree[child_id] == 0) {
                    q.push(child_id);
                }
            }
        }

        return visited_count == twrs_.size(); // true if acyclic (DAG)
    }

private:
    std::unordered_map<ResourceId, std::unique_ptr<VersionedResource>> resources_;
    std::unordered_map<TWRId, std::unique_ptr<TemporalWorkRegion>> twrs_;
};

} // namespace twrf
