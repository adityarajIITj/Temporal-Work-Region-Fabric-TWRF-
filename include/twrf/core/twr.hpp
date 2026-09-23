#pragma once

#include "twrf/core/types.hpp"
#include "twrf/core/resource.hpp"
#include "twrf/core/state_store.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_set>
#include <sstream>

namespace twrf {

struct ResourceBinding {
    ResourceId resource_id{0};
    VersionNumber recorded_version{INVALID_VERSION};
};

struct UpstreamBinding {
    TWRId producer_id{0};
    VersionNumber recorded_version{INVALID_VERSION};
};

class TemporalWorkRegion;

using KernelCallback = std::function<bool(TemporalWorkRegion& self,
                                          LogicalStateStore& state_store,
                                          const std::vector<const VersionedResource*>& inputs)>;

class TemporalWorkRegion {
public:
    TemporalWorkRegion(TWRId id, std::string name, BoundingRegion region = BoundingRegion::full_screen(), int32_t priority = 0)
        : id_(id), name_(std::move(name)), region_(region), priority_(priority) {}

    [[nodiscard]] TWRId id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const BoundingRegion& region() const noexcept { return region_; }
    [[nodiscard]] TWRStatus status() const noexcept { return status_; }
    [[nodiscard]] ExecutionReason execution_reason() const noexcept { return execution_reason_; }
    [[nodiscard]] int32_t priority() const noexcept { return priority_; }
    [[nodiscard]] uint32_t topological_depth() const noexcept { return topological_depth_; }
    [[nodiscard]] uint32_t pending_dependencies() const noexcept { return pending_dependencies_; }
    [[nodiscard]] VersionNumber current_output_version() const noexcept { return current_output_version_; }
    [[nodiscard]] uint64_t total_executions() const noexcept { return total_executions_; }
    [[nodiscard]] uint64_t total_skips() const noexcept { return total_skips_; }

    // Opt-in dependency auditing. Kernels call observe_resource() whenever
    // they consume a mutable VersionedResource.
    void enable_dependency_audit(bool enabled = true) noexcept {
        dependency_audit_enabled_ = enabled;
    }

    [[nodiscard]] bool dependency_audit_enabled() const noexcept {
        return dependency_audit_enabled_;
    }

    void observe_resource(ResourceId id) {
        if (dependency_audit_enabled_) {
            observed_resource_reads_.insert(id);
        }
    }

    void observe_upstream_producer(TWRId producer_id) {
        if (dependency_audit_enabled_) {
            observed_producer_reads_.insert(producer_id);
        }
    }

    [[nodiscard]] const std::unordered_set<ResourceId>& observed_resource_reads() const noexcept {
        return observed_resource_reads_;
    }

    [[nodiscard]] bool dependency_audit_passes() const noexcept {
        for (ResourceId observed : observed_resource_reads_) {
            bool declared = false;
            for (const auto& binding : resource_bindings_) {
                if (binding.resource_id == observed) {
                    declared = true;
                    break;
                }
            }
            if (!declared) return false;
        }
        for (TWRId observed : observed_producer_reads_) {
            bool declared = false;
            for (const auto& binding : upstream_producers_) {
                if (binding.producer_id == observed) {
                    declared = true;
                    break;
                }
            }
            if (!declared) return false;
        }
        return true;
    }

    [[nodiscard]] std::string dependency_audit_report() const {
        std::ostringstream out;
        out << "TWR " << id_ << " dependency audit: ";
        if (dependency_audit_passes()) {
            out << "PASS";
            return out.str();
        }
        out << "FAIL; undeclared dependencies:";
        bool first = true;
        for (ResourceId observed : observed_resource_reads_) {
            bool declared = false;
            for (const auto& binding : resource_bindings_) {
                if (binding.resource_id == observed) { declared = true; break; }
            }
            if (!declared) {
                out << (first ? " resource=" : ", resource=") << observed;
                first = false;
            }
        }
        for (TWRId observed : observed_producer_reads_) {
            bool declared = false;
            for (const auto& binding : upstream_producers_) {
                if (binding.producer_id == observed) { declared = true; break; }
            }
            if (!declared) {
                out << (first ? " producer=" : ", producer=") << observed;
                first = false;
            }
        }
        return out.str();
    }

    [[nodiscard]] std::string dependency_audit_report_old() const {
        std::ostringstream out;
        out << "TWR " << id_ << " dependency audit: ";
        if (dependency_audit_passes()) {
            out << "PASS";
            return out.str();
        }
        out << "FAIL; undeclared resources:";
        for (ResourceId observed : observed_resource_reads_) {
            bool declared = false;
            for (const auto& binding : resource_bindings_) {
                if (binding.resource_id == observed) {
                    declared = true;
                    break;
                }
            }
            if (!declared) out << " " << observed;
        }
        return out.str();
    }


    [[nodiscard]] const std::vector<ResourceBinding>& resource_bindings() const noexcept { return resource_bindings_; }
    [[nodiscard]] const std::vector<UpstreamBinding>& upstream_producers() const noexcept { return upstream_producers_; }
    [[nodiscard]] const std::vector<TWRId>& downstream_consumers() const noexcept { return downstream_consumers_; }

    void set_region(const BoundingRegion& region) noexcept { region_ = region; }
    void set_priority(int32_t priority) noexcept { priority_ = priority; }
    void set_topological_depth(uint32_t depth) noexcept { topological_depth_ = depth; }
    void set_kernel(KernelCallback kernel) { kernel_ = std::move(kernel); }

    void bind_resource(ResourceId id, VersionNumber recorded = INVALID_VERSION) {
        resource_bindings_.push_back({id, recorded});
    }

    void add_upstream_dependency(TWRId producer_id, VersionNumber recorded = INVALID_VERSION) {
        upstream_producers_.push_back({producer_id, recorded});
    }

    void add_downstream_consumer(TWRId consumer_id) {
        downstream_consumers_.push_back(consumer_id);
    }

    void mark_dirty(ExecutionReason reason) noexcept {
        status_ = TWRStatus::Dirty;
        execution_reason_ = reason;
    }

    void set_ready() noexcept {
        status_ = TWRStatus::Ready;
    }

    void set_executing() noexcept {
        status_ = TWRStatus::Executing;
    }

    void mark_clean() noexcept {
        status_ = TWRStatus::IdleClean;
        execution_reason_ = ExecutionReason::None;
    }

    void mark_failed(ExecutionReason reason = ExecutionReason::None) noexcept {
        status_ = TWRStatus::Failed;
        if (reason != ExecutionReason::None) {
            execution_reason_ = reason;
        }
    }

    void record_skip() noexcept {
        total_skips_++;
    }

    void reset_pending_dependencies() noexcept {
        pending_dependencies_ = static_cast<uint32_t>(upstream_producers_.size());
    }

    void set_pending_dependencies(uint32_t count) noexcept {
        pending_dependencies_ = count;
    }

    void decrement_pending_dependencies() noexcept {
        if (pending_dependencies_ > 0) {
            pending_dependencies_--;
        }
    }

    bool execute(LogicalStateStore& state_store,
                 const std::vector<const VersionedResource*>& inputs,
                 [[maybe_unused]] Timestamp step) {
        if (!kernel_) return false;
        set_executing();
        total_executions_++;
        observed_resource_reads_.clear();
        observed_producer_reads_.clear();

        bool ok = kernel_(*this, state_store, inputs);
        if (ok) {
            if (dependency_audit_enabled_ && !dependency_audit_passes()) {
                mark_failed(ExecutionReason::InputVersionChanged);
                return false;
            }
            current_output_version_++;
            // Record versions only after successful computation and State Store commit.
            for (size_t i = 0; i < resource_bindings_.size() && i < inputs.size(); ++i) {
                if (inputs[i]) {
                    resource_bindings_[i].recorded_version = inputs[i]->version();
                }
            }
            for (auto& up : upstream_producers_) {
                up.recorded_version = state_store.get_output_version(up.producer_id);
            }
            mark_clean();
        } else {
            // Failed execution is never valid and cannot release consumers.
            mark_failed(ExecutionReason::None);
        }
        return ok;
    }

private:
    TWRId id_;
    std::string name_;
    BoundingRegion region_;
    int32_t priority_{0};
    uint32_t topological_depth_{0};
    uint32_t pending_dependencies_{0};

    TWRStatus status_{TWRStatus::Dirty};
    ExecutionReason execution_reason_{ExecutionReason::Initial};

    VersionNumber current_output_version_{INITIAL_VERSION};
    uint64_t total_executions_{0};
    uint64_t total_skips_{0};

    std::vector<ResourceBinding> resource_bindings_;
    std::vector<UpstreamBinding> upstream_producers_;
    std::vector<TWRId> downstream_consumers_;

    KernelCallback kernel_;

    bool dependency_audit_enabled_{false};
    std::unordered_set<ResourceId> observed_resource_reads_;
    std::unordered_set<TWRId> observed_producer_reads_;
};

} // namespace twrf
