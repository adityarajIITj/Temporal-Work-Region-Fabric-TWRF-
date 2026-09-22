#include "twrf/neural/mlp.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S4-03: Temporal Neural State Persistence & Weight Invalidation");

    twrf::neural::TemporalMLPPipeline pipeline(4 /* input */, 8 /* hidden */, 4 /* output */);
    pipeline.initialize();

    // 1. Initial frame: TWR executes and initializes persistent state
    pipeline.set_input(twrf::neural::Tensor1D{0.5f, 1.0f, -0.2f, 0.8f});
    uint64_t exec1 = pipeline.run_step();
    TWRF_ASSERT(exec1 == 1, "S4-03 Failed: Neural TWR must execute on frame 1");

    auto out1 = pipeline.get_current_output();
    auto h1 = pipeline.hidden_state();

    // Verify State Store has recorded output
    size_t sz = 0;
    twrf::VersionNumber ver = 0;
    const uint8_t* raw = pipeline.state_store().read_output(twrf::neural::NEURAL_TWR_ID, sz, ver);
    TWRF_ASSERT(raw != nullptr && sz > 0, "S4-03 Failed: Persistent output must exist in State Store");

    // 2. Static step: identical input, no weight change -> TWR skipped!
    uint64_t exec2 = pipeline.run_step();
    TWRF_ASSERT(exec2 == 0, "S4-03 Failed: Neural TWR must skip execution when inputs/weights are static");
    auto out2 = pipeline.get_current_output();
    TWRF_ASSERT(std::abs(out1[0] - out2[0]) < 1e-6f, "S4-03 Failed: Skipped output must be identical");

    // 3. Temporal evolution: new input at frame 3
    pipeline.set_input(twrf::neural::Tensor1D{0.8f, -0.5f, 0.3f, 1.2f});
    uint64_t exec3 = pipeline.run_step();
    TWRF_ASSERT(exec3 == 1, "S4-03 Failed: New input must trigger execution");

    auto out3 = pipeline.get_current_output();
    auto h3 = pipeline.hidden_state();
    // Hidden state must evolve monotonically and be distinct from h1
    bool hidden_changed = false;
    for (size_t i = 0; i < h3.size(); ++i) {
        if (std::abs(h3[i] - h1[i]) > 1e-4f) hidden_changed = true;
    }
    TWRF_ASSERT(hidden_changed, "S4-03 Failed: Temporal hidden state must evolve across frame steps");

    // 4. Weight update: invalidates Neural TWR even if input is static
    pipeline.notify_weights_changed();
    uint64_t exec4 = pipeline.run_step();
    TWRF_ASSERT(exec4 == 1, "S4-03 Failed: Weight update must invalidate neural TWR and trigger execution");

    TWRF_TEST_PASS("S4-03: Temporal Neural State Persistence & Weight Invalidation");
    return 0;
}
