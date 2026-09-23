#pragma once

#include "twrf/core/types.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace twrf {

struct TraceEntry {
    Timestamp step{0};
    TWRId twr_id{0};
    std::string twr_name;
    ExecutionReason reason{ExecutionReason::None};
    bool executed{false};
    VersionNumber output_version{INVALID_VERSION};
    std::vector<std::pair<ResourceId, VersionNumber>> input_versions;
};

class ExecutionTrace {
public:
    void record_execution(Timestamp step, TWRId twr_id, std::string twr_name,
                          ExecutionReason reason, VersionNumber out_ver,
                          std::vector<std::pair<ResourceId, VersionNumber>> in_vers) {
        entries_.push_back(TraceEntry{
            step, twr_id, std::move(twr_name), reason, true, out_ver, std::move(in_vers)
        });
    }

    void record_skip(Timestamp step, TWRId twr_id, std::string twr_name,
                     std::vector<std::pair<ResourceId, VersionNumber>> in_vers) {
        entries_.push_back(TraceEntry{
            step, twr_id, std::move(twr_name), ExecutionReason::None, false, INVALID_VERSION, std::move(in_vers)
        });
    }

    [[nodiscard]] const std::vector<TraceEntry>& entries() const noexcept {
        return entries_;
    }


    [[nodiscard]] std::vector<TWRId> executed_ids(Timestamp step) const {
        std::vector<TWRId> ids;
        for (const auto& e : entries_) {
            if (e.step == step && e.executed) ids.push_back(e.twr_id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    [[nodiscard]] std::vector<TWRId> executed_ids_since(size_t begin_index) const {
        std::vector<TWRId> ids;
        for (size_t i = begin_index; i < entries_.size(); ++i) {
            if (entries_[i].executed) ids.push_back(entries_[i].twr_id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    [[nodiscard]] bool execution_set_equals(const ExecutionTrace& other,
                                            Timestamp step) const {
        return executed_ids(step) == other.executed_ids(step);
    }

    [[nodiscard]] bool execution_set_equals_since(const ExecutionTrace& other,
                                                  size_t begin_index,
                                                  size_t other_begin_index) const {
        return executed_ids_since(begin_index) ==
               other.executed_ids_since(other_begin_index);
    }


    void clear() noexcept {
        entries_.clear();
    }

    [[nodiscard]] std::string to_string() const {
        std::ostringstream oss;
        oss << "Step | TWR | Name                 | Status   | Reason                 | OutVer\n";
        oss << "-----+-----+----------------------+----------+------------------------+-------\n";
        for (const auto& e : entries_) {
            oss << std::setw(4) << e.step << " | "
                << std::setw(3) << e.twr_id << " | "
                << std::setw(20) << e.twr_name << " | "
                << std::setw(8) << (e.executed ? "EXEC" : "SKIP") << " | "
                << std::setw(22) << twrf::to_string(e.reason) << " | "
                << std::setw(6) << e.output_version << "\n";
        }
        return oss.str();
    }

private:
    std::vector<TraceEntry> entries_;
};

} // namespace twrf
