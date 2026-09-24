#pragma once

#include "twrf/neural/tensor.hpp"
#include "twrf/core/twr.hpp"
#include "twrf/core/graph.hpp"
#include "twrf/core/state_store.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/trace.hpp"
#include "twrf/core/metrics.hpp"
#include <vector>
#include <memory>
#include <cstring>

namespace twrf::neural {

constexpr ResourceId NEURAL_WEIGHTS_RESOURCE_ID = 6000;
constexpr ResourceId NEURAL_INPUT_RESOURCE_ID = 6001;
constexpr TWRId NEURAL_TWR_ID = 30000;

struct MLPLayer {
    Tensor2D weights;
    Tensor1D bias;
    bool use_relu{true};

    MLPLayer() = default;
    MLPLayer(size_t in_dim, size_t out_dim, bool relu = true, float weight_init = 0.1f)
        : weights(out_dim, in_dim, weight_init), bias(out_dim, 0.01f), use_relu(relu) {}

    [[nodiscard]] Tensor1D forward(const Tensor1D& x) const {
        Tensor1D out = weights.mat_vec(x) + bias;
        if (use_relu) {
            out.relu_inplace();
        }
        return out;
    }
};

class MLPNetwork {
public:
    MLPNetwork() = default;

    void add_layer(const MLPLayer& layer) {
        layers_.push_back(layer);
    }

    [[nodiscard]] const std::vector<MLPLayer>& layers() const noexcept {
        return layers_;
    }

    [[nodiscard]] std::vector<MLPLayer>& layers() noexcept {
        return layers_;
    }

    [[nodiscard]] Tensor1D forward(const Tensor1D& input) const {
        Tensor1D current = input;
        for (const auto& l : layers_) {
            current = l.forward(current);
        }
        return current;
    }

    // Flattens all layer parameters into a single byte stream
    [[nodiscard]] std::vector<uint8_t> serialize_weights() const {
        std::vector<float> floats;
        for (const auto& l : layers_) {
            floats.insert(floats.end(), l.weights.data.begin(), l.weights.data.end());
            floats.insert(floats.end(), l.bias.data.begin(), l.bias.data.end());
        }
        std::vector<uint8_t> bytes(floats.size() * sizeof(float));
        std::memcpy(bytes.data(), floats.data(), bytes.size());
        return bytes;
    }

private:
    std::vector<MLPLayer> layers_;
};

// Stateful Temporal MLP Pipeline operating on persistent TWR State Store
class TemporalMLPPipeline {
public:
    TemporalMLPPipeline(size_t input_dim = 4, size_t hidden_dim = 8, size_t output_dim = 4)
        : input_dim_(input_dim), hidden_dim_(hidden_dim), output_dim_(output_dim) {
        graph_ = std::make_unique<TWRGraph>();

        // Build a 2-layer MLP: (input + hidden) -> hidden -> output
        // First layer takes [input_t, hidden_{t-1}]
        model_.add_layer(MLPLayer(input_dim + hidden_dim, hidden_dim, true, 0.05f));
        model_.add_layer(MLPLayer(hidden_dim, output_dim + hidden_dim, false, 0.05f));

        current_input_ = Tensor1D(input_dim_, 1.0f);
        hidden_state_ = Tensor1D(hidden_dim_, 0.0f);
    }

    [[nodiscard]] MLPNetwork& model() noexcept { return model_; }
    [[nodiscard]] const MLPNetwork& model() const noexcept { return model_; }
    [[nodiscard]] const Tensor1D& hidden_state() const noexcept { return hidden_state_; }
    [[nodiscard]] TWRGraph& graph() noexcept { return *graph_; }
    [[nodiscard]] LogicalStateStore& state_store() noexcept { return state_store_; }
    [[nodiscard]] const MetricsCollector& metrics() const noexcept { return metrics_; }

    void initialize() {
        // Register versioned weight resource (read-only)
        auto& weight_res = graph_->add_resource(NEURAL_WEIGHTS_RESOURCE_ID, "MLPWeights");
        auto w_bytes = model_.serialize_weights();
        weight_res.set_data(w_bytes.data(), w_bytes.size());

        // Register versioned input resource
        auto& in_res = graph_->add_resource(NEURAL_INPUT_RESOURCE_ID, "MLPInputs");
        in_res.set_data(current_input_.data.data(), current_input_.size() * sizeof(float));

        // Create Neural TWR
        auto& twr = graph_->add_twr(NEURAL_TWR_ID, "TemporalMLP_Node");
        graph_->bind_resource(NEURAL_TWR_ID, NEURAL_WEIGHTS_RESOURCE_ID);
        graph_->bind_resource(NEURAL_TWR_ID, NEURAL_INPUT_RESOURCE_ID);

        // Kernel reads persistent state from previous frame, computes MLP, and writes back state
        twr.set_kernel([this](TemporalWorkRegion& self, LogicalStateStore& store,
                              const std::vector<const VersionedResource*>&) -> bool {
            self.observe_resource(NEURAL_WEIGHTS_RESOURCE_ID);
            self.observe_resource(NEURAL_INPUT_RESOURCE_ID);

            // Read previous hidden state from State Store if exists
            size_t sz = 0;
            VersionNumber ver = 0;
            const uint8_t* prev_state_raw = store.read_output(self.id(), sz, ver);

            Tensor1D prev_hidden(hidden_dim_, 0.0f);
            if (prev_state_raw && sz >= (output_dim_ + hidden_dim_) * sizeof(float)) {
                const float* ptr = reinterpret_cast<const float*>(prev_state_raw);
                // Last hidden_dim_ floats are persistent hidden state
                std::memcpy(prev_hidden.data.data(), ptr + output_dim_, hidden_dim_ * sizeof(float));
            }

            // Concatenate [current_input, prev_hidden]
            Tensor1D full_input(input_dim_ + hidden_dim_);
            for (size_t i = 0; i < input_dim_; ++i) full_input[i] = current_input_[i];
            for (size_t i = 0; i < hidden_dim_; ++i) full_input[input_dim_ + i] = prev_hidden[i];

            // Run MLP
            Tensor1D full_output = model_.forward(full_input);

            // Update persistent hidden state cache
            for (size_t i = 0; i < hidden_dim_; ++i) {
                hidden_state_[i] = full_output[output_dim_ + i];
            }

            // Write full output [output_t, hidden_t] to State Store
            if (!store.write_output(self.id(), full_output.data.data(),
                               full_output.size() * sizeof(float),
                               self.current_output_version() + 1)) return false;

            return true;
        });

        for (const auto& [id, twr] : graph_->twrs()) twr->enable_dependency_audit(true);
        graph_->validate_and_compute_depths();
    }

    void set_input(const Tensor1D& in) {
        current_input_ = in;
        auto* res = graph_->get_resource(NEURAL_INPUT_RESOURCE_ID);
        if (res) {
            res->set_data(current_input_.data.data(), current_input_.size() * sizeof(float));
        }
    }

    void notify_weights_changed() {
        auto* res = graph_->get_resource(NEURAL_WEIGHTS_RESOURCE_ID);
        if (res) {
            auto w_bytes = model_.serialize_weights();
            res->set_data(w_bytes.data(), w_bytes.size());
        }
    }

    uint64_t run_step() {
        uint64_t exec_before = metrics_.total_twr_executions;
        scheduler_.run_frame(*graph_, state_store_, trace_, metrics_);
        return metrics_.total_twr_executions - exec_before;
    }

    // Reads current output from State Store
    [[nodiscard]] Tensor1D get_current_output() const {
        size_t sz = 0;
        VersionNumber ver = 0;
        const uint8_t* raw = state_store_.read_output(NEURAL_TWR_ID, sz, ver);
        if (!raw || sz < output_dim_ * sizeof(float)) {
            return Tensor1D(output_dim_, 0.0f);
        }
        Tensor1D out(output_dim_);
        std::memcpy(out.data.data(), raw, output_dim_ * sizeof(float));
        return out;
    }

private:
    size_t input_dim_{4};
    size_t hidden_dim_{8};
    size_t output_dim_{4};

    MLPNetwork model_;
    Tensor1D current_input_;
    Tensor1D hidden_state_;

    std::unique_ptr<TWRGraph> graph_;
    LogicalStateStore state_store_;
    TWRScheduler scheduler_;
    ExecutionTrace trace_;
    MetricsCollector metrics_;
};

} // namespace twrf::neural
