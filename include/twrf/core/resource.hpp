#pragma once

#include "twrf/core/types.hpp"
#include <string>
#include <vector>
#include <cstring>
#include <memory>

namespace twrf {

class VersionedResource {
public:
    VersionedResource(ResourceId id, std::string name, BoundingRegion bounds = BoundingRegion::full_screen())
        : id_(id), name_(std::move(name)), version_(INITIAL_VERSION), bounds_(bounds) {}

    [[nodiscard]] ResourceId id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] VersionNumber version() const noexcept { return version_; }
    [[nodiscard]] const BoundingRegion& bounds() const noexcept { return bounds_; }
    [[nodiscard]] const std::vector<uint8_t>& data() const noexcept { return data_; }
    [[nodiscard]] size_t size_bytes() const noexcept { return data_.size(); }

    void set_bounds(const BoundingRegion& bounds) noexcept {
        bounds_ = bounds;
    }

    VersionNumber bump_version() noexcept {
        return ++version_;
    }

    void set_data(const void* src, size_t size) {
        data_.resize(size);
        if (size > 0 && src != nullptr) {
            std::memcpy(data_.data(), src, size);
        }
        bump_version();
    }

    template <typename T>
    void set_value(const T& val) {
        set_data(&val, sizeof(T));
    }

    template <typename T>
    [[nodiscard]] const T* as() const noexcept {
        if (data_.size() < sizeof(T)) return nullptr;
        return reinterpret_cast<const T*>(data_.data());
    }

    template <typename T>
    [[nodiscard]] T get_value(const T& fallback = T{}) const noexcept {
        const T* ptr = as<T>();
        return ptr ? *ptr : fallback;
    }

private:
    ResourceId id_;
    std::string name_;
    VersionNumber version_;
    BoundingRegion bounds_;
    std::vector<uint8_t> data_;
};

} // namespace twrf
