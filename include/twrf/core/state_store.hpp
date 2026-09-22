#pragma once

#include "twrf/core/types.hpp"
#include <unordered_map>
#include <vector>
#include <cstddef>
#include <cstring>
#include <stdexcept>

namespace twrf {

struct TWRStateSlot {
    VersionNumber output_version{INVALID_VERSION};
    std::vector<uint8_t> payload;
    Timestamp last_updated_step{0};
};

struct StateStoreMetrics {
    size_t current_allocated_bytes{0};
    size_t peak_allocated_bytes{0};
    uint64_t total_reads{0};
    uint64_t total_writes{0};
    uint64_t total_read_bytes{0};
    uint64_t total_write_bytes{0};
    uint64_t capacity_limit_bytes{0}; // 0 = unlimited / parameter tracking only
};

class LogicalStateStore {
public:
    explicit LogicalStateStore(size_t capacity_limit_bytes = 0)
        : metrics_{0, 0, 0, 0, 0, 0, capacity_limit_bytes} {}

    void set_capacity_limit(size_t limit_bytes) noexcept {
        metrics_.capacity_limit_bytes = limit_bytes;
    }

    [[nodiscard]] const StateStoreMetrics& metrics() const noexcept {
        return metrics_;
    }

    void reset_metrics() noexcept {
        metrics_.total_reads = 0;
        metrics_.total_writes = 0;
        metrics_.total_read_bytes = 0;
        metrics_.total_write_bytes = 0;
    }

    bool has_slot(TWRId id) const noexcept {
        return slots_.find(id) != slots_.end();
    }

    void allocate_slot(TWRId id, size_t initial_capacity = 0) {
        auto& slot = slots_[id];
        if (initial_capacity > 0 && slot.payload.capacity() < initial_capacity) {
            slot.payload.reserve(initial_capacity);
        }
    }

    bool write_output(TWRId id, const void* src, size_t size_bytes, VersionNumber version, Timestamp step = 0) {
        auto& slot = slots_[id];
        size_t old_size = slot.payload.size();

        if (metrics_.capacity_limit_bytes > 0) {
            size_t projected = metrics_.current_allocated_bytes - old_size + size_bytes;
            if (projected > metrics_.capacity_limit_bytes) {
                return false; // Out of capacity limit
            }
        }

        metrics_.current_allocated_bytes = metrics_.current_allocated_bytes - old_size + size_bytes;
        if (metrics_.current_allocated_bytes > metrics_.peak_allocated_bytes) {
            metrics_.peak_allocated_bytes = metrics_.current_allocated_bytes;
        }

        metrics_.total_writes++;
        metrics_.total_write_bytes += size_bytes;

        slot.payload.resize(size_bytes);
        if (size_bytes > 0 && src != nullptr) {
            std::memcpy(slot.payload.data(), src, size_bytes);
        }
        slot.output_version = version;
        slot.last_updated_step = step;

        return true;
    }

    [[nodiscard]] const uint8_t* read_output(TWRId id, size_t& out_size, VersionNumber& out_version) const {
        auto it = slots_.find(id);
        if (it == slots_.end()) {
            out_size = 0;
            out_version = INVALID_VERSION;
            return nullptr;
        }

        metrics_.total_reads++;
        metrics_.total_read_bytes += it->second.payload.size();

        out_size = it->second.payload.size();
        out_version = it->second.output_version;
        return it->second.payload.data();
    }

    [[nodiscard]] VersionNumber get_output_version(TWRId id) const noexcept {
        auto it = slots_.find(id);
        if (it == slots_.end()) return INVALID_VERSION;
        return it->second.output_version;
    }

    [[nodiscard]] size_t slot_count() const noexcept {
        return slots_.size();
    }

private:
    mutable StateStoreMetrics metrics_;
    std::unordered_map<TWRId, TWRStateSlot> slots_;
};

} // namespace twrf
