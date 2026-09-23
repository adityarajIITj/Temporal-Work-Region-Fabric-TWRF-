#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <string_view>
#include <algorithm>

namespace twrf {

using TWRId = uint32_t;
using ResourceId = uint32_t;
using VersionNumber = uint64_t;
using Timestamp = uint64_t;

constexpr VersionNumber INITIAL_VERSION = 1;
constexpr VersionNumber INVALID_VERSION = 0;

enum class TWRStatus {
    IdleClean,
    Dirty,
    Ready,
    Executing,
    Failed
};

enum class ExecutionReason {
    None,                    // Not executed / skipped
    Initial,                 // First frame allocation / initial execution
    InputVersionChanged,     // Direct external resource version changed
    ProducerOutputChanged,   // Upstream producer TWR produced new output version
    ConservativeInvalidation,// Conservative bounding box or spatial filter triggered
    ForcedRecompute          // Oracle forced full execution
};

inline std::string_view to_string(TWRStatus status) {
    switch (status) {
        case TWRStatus::IdleClean: return "IdleClean";
        case TWRStatus::Dirty:     return "Dirty";
        case TWRStatus::Ready:     return "Ready";
        case TWRStatus::Executing: return "Executing";
        case TWRStatus::Failed:    return "Failed";
    }
    return "Unknown";
}

inline std::string_view to_string(ExecutionReason reason) {
    switch (reason) {
        case ExecutionReason::None:                     return "None";
        case ExecutionReason::Initial:                  return "Initial";
        case ExecutionReason::InputVersionChanged:      return "InputVersionChanged";
        case ExecutionReason::ProducerOutputChanged:    return "ProducerOutputChanged";
        case ExecutionReason::ConservativeInvalidation: return "ConservativeInvalidation";
        case ExecutionReason::ForcedRecompute:          return "ForcedRecompute";
    }
    return "Unknown";
}

struct BoundingRegion {
    int32_t min_x{0};
    int32_t min_y{0};
    int32_t max_x{0};
    int32_t max_y{0};

    [[nodiscard]] bool is_empty() const noexcept {
        return min_x >= max_x || min_y >= max_y;
    }

    [[nodiscard]] bool overlaps(const BoundingRegion& other) const noexcept {
        if (is_empty() || other.is_empty()) return false;
        return (min_x < other.max_x && max_x > other.min_x &&
                min_y < other.max_y && max_y > other.min_y);
    }

    static BoundingRegion create(int32_t x0, int32_t y0, int32_t x1, int32_t y1) noexcept {
        return BoundingRegion{x0, y0, x1, y1};
    }

    static BoundingRegion full_screen(int32_t width = 1920, int32_t height = 1080) noexcept {
        return BoundingRegion{0, 0, width, height};
    }
};

} // namespace twrf
