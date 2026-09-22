#include "twrf/neural/tensor.hpp"
#include "twrf/neural/mlp.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S4-02: Deterministic Neural Inference & Reference Parity");

    // Construct a deterministic 2-layer MLP: 4 -> 4 (ReLU) -> 2 (Linear)
    twrf::neural::MLPNetwork mlp;

    twrf::neural::MLPLayer layer1(4, 4, true /* ReLU */, 0.0f);
    // Explicit weights for layer 1
    float l1_w[] = {
        0.5f, -0.2f,  0.1f,  0.4f,
       -0.1f,  0.8f,  0.3f, -0.5f,
        0.2f,  0.1f, -0.7f,  0.9f,
       -0.4f, -0.3f,  0.6f,  0.2f
    };
    std::memcpy(layer1.weights.data.data(), l1_w, sizeof(l1_w));
    layer1.bias = twrf::neural::Tensor1D{0.1f, -0.05f, 0.2f, -0.1f};
    mlp.add_layer(layer1);

    twrf::neural::MLPLayer layer2(4, 2, false /* Linear */, 0.0f);
    float l2_w[] = {
        0.3f, -0.5f,  0.2f,  0.7f,
       -0.2f,  0.4f,  0.6f, -0.1f
    };
    std::memcpy(layer2.weights.data.data(), l2_w, sizeof(l2_w));
    layer2.bias = twrf::neural::Tensor1D{0.05f, -0.02f};
    mlp.add_layer(layer2);

    // Input vector
    twrf::neural::Tensor1D input{1.0f, 0.5f, -1.0f, 2.0f};

    // Reference calculation by hand:
    // Layer 1 pre-activation:
    // row 0: 0.5*1.0 + -0.2*0.5 +  0.1*-1.0 +  0.4*2.0 + 0.1  = 0.5 - 0.1 - 0.1 + 0.8 + 0.1 = 1.2
    // row 1: -0.1*1.0 + 0.8*0.5 +  0.3*-1.0 + -0.5*2.0 - 0.05 = -0.1 + 0.4 - 0.3 - 1.0 - 0.05 = -1.05
    // row 2: 0.2*1.0 +  0.1*0.5 + -0.7*-1.0 +  0.9*2.0 + 0.2  = 0.2 + 0.05 + 0.7 + 1.8 + 0.2 = 2.95
    // row 3: -0.4*1.0 + -0.3*0.5 + 0.6*-1.0 +  0.2*2.0 - 0.1  = -0.4 - 0.15 - 0.6 + 0.4 - 0.1 = -0.85
    // ReLU activations: [1.2, 0.0, 2.95, 0.0]
    // Layer 2 pre-activation:
    // row 0: 0.3*1.2 - 0.5*0.0 + 0.2*2.95 + 0.7*0.0 + 0.05 = 0.36 + 0.59 + 0.05 = 1.00
    // row 1: -0.2*1.2 + 0.4*0.0 + 0.6*2.95 - 0.1*0.0 - 0.02 = -0.24 + 1.77 - 0.02 = 1.51
    float expected_out0 = 1.00f;
    float expected_out1 = 1.51f;

    auto result = mlp.forward(input);

    TWRF_ASSERT(result.size() == 2, "S4-02 Failed: Output dimension mismatch");
    TWRF_ASSERT(std::abs(result[0] - expected_out0) < 1e-5f,
                "S4-02 Failed: Layer 2 output[0] differs from reference calculation");
    TWRF_ASSERT(std::abs(result[1] - expected_out1) < 1e-5f,
                "S4-02 Failed: Layer 2 output[1] differs from reference calculation");

    TWRF_TEST_PASS("S4-02: Deterministic Neural Inference & Reference Parity");
    return 0;
}
