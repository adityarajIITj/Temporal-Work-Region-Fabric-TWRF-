#pragma once

#include "twrf/core/types.hpp"
#include "twrf/core/twr.hpp"
#include "twrf/core/graph.hpp"
#include "twrf/core/state_store.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/trace.hpp"
#include "twrf/core/metrics.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace twrf::api {

enum class CommandType {
    CreateResource,
    UpdateResource,
    CreateRegion,
    BindResource,
    BindProducer,
    SubmitFrame
};

struct SerializedCommand {
    CommandType type;
    ResourceId resource_id{0};
    TWRId twr_id{0};
    TWRId producer_id{0};
    std::string name;
    std::vector<uint8_t> payload;
};

// Architecture-Native Command Layer for TWRF Virtual GPU
class TWRFContext {
public:
    TWRFContext() {
        graph_ = std::make_unique<TWRGraph>();
    }

    // 1. Resource Management
    VersionedResource& create_resource(ResourceId id, std::string name, BoundingRegion bounds = BoundingRegion::full_screen()) {
        record_command(SerializedCommand{CommandType::CreateResource, id, 0, 0, name, {}});
        return graph_->add_resource(id, std::move(name), bounds);
    }

    void update_resource_data(ResourceId id, const void* data, size_t size) {
        auto* res = graph_->get_resource(id);
        if (res) {
            std::vector<uint8_t> bytes(size);
            if (data && size > 0) std::memcpy(bytes.data(), data, size);
            record_command(SerializedCommand{CommandType::UpdateResource, id, 0, 0, "", bytes});
            res->set_data(data, size);
        }
    }

    template <typename T>
    void update_resource_value(ResourceId id, const T& val) {
        update_resource_data(id, &val, sizeof(T));
    }

    // 2. Temporal Work Region Creation & Wiring
    TemporalWorkRegion& create_region(TWRId id, std::string name, BoundingRegion region = BoundingRegion::full_screen()) {
        record_command(SerializedCommand{CommandType::CreateRegion, 0, id, 0, name, {}});
        return graph_->add_twr(id, std::move(name), region);
    }

    void bind_resource_input(TWRId twr_id, ResourceId res_id) {
        record_command(SerializedCommand{CommandType::BindResource, res_id, twr_id, 0, "", {}});
        graph_->bind_resource(twr_id, res_id);
    }

    void bind_upstream_producer(TWRId consumer_id, TWRId producer_id) {
        record_command(SerializedCommand{CommandType::BindProducer, 0, consumer_id, producer_id, "", {}});
        graph_->connect_dependency(producer_id, consumer_id);
    }

    void set_region_kernel(TWRId twr_id, KernelCallback kernel) {
        auto* twr = graph_->get_twr(twr_id);
        if (twr) {
            twr->set_kernel(std::move(kernel));
        }
    }

    void compile_graph() {
        graph_->validate_and_compute_depths();
    }

    // 3. Execution
    uint64_t submit_frame() {
        record_command(SerializedCommand{CommandType::SubmitFrame, 0, 0, 0, "", {}});
        uint64_t exec_before = metrics_.total_twr_executions;
        scheduler_.run_frame(*graph_, state_store_, trace_, metrics_);
        return metrics_.total_twr_executions - exec_before;
    }

    // 4. Output State Retrieval
    [[nodiscard]] const uint8_t* read_state(TWRId twr_id, size_t& out_size, VersionNumber& out_version) const {
        return state_store_.read_output(twr_id, out_size, out_version);
    }

    [[nodiscard]] const ExecutionTrace& trace() const noexcept { return trace_; }
    [[nodiscard]] const MetricsCollector& metrics() const noexcept { return metrics_; }
    [[nodiscard]] const std::vector<SerializedCommand>& command_stream() const noexcept { return command_stream_; }

    void clear_command_stream() noexcept {
        command_stream_.clear();
    }

private:
    void record_command(SerializedCommand cmd) {
        command_stream_.push_back(std::move(cmd));
    }

    std::unique_ptr<TWRGraph> graph_;
    LogicalStateStore state_store_;
    TWRScheduler scheduler_;
    ExecutionTrace trace_;
    MetricsCollector metrics_;

    std::vector<SerializedCommand> command_stream_;
};

} // namespace twrf::api
