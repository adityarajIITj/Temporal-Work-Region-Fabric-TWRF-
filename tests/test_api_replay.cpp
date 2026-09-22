#include "twrf/api/twrf_api.hpp"
#include "tests/test_common.hpp"

struct ApiScenarioOutput {
    std::string trace_string;
    uint64_t total_executions{0};
    uint64_t total_skips{0};
    float state_val{0.0f};
};

ApiScenarioOutput run_api_scenario() {
    twrf::api::TWRFContext ctx;

    // 1. Create resources
    ctx.create_resource(1, "ResAlpha");
    ctx.create_resource(2, "ResBeta");
    ctx.update_resource_value(1, 42.0f);
    ctx.update_resource_value(2, 10.0f);

    // 2. Create and wire TWRs
    ctx.create_region(10, "RegionA");
    ctx.bind_resource_input(10, 1);
    ctx.set_region_kernel(10, [](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& store,
                                 const std::vector<const twrf::VersionedResource*>& inputs) -> bool {
        float in_val = inputs.empty() || !inputs[0] ? 0.0f : inputs[0]->get_value<float>(0.0f);
        float out_val = in_val * 2.0f;
        store.write_output(self.id(), &out_val, sizeof(float), self.current_output_version() + 1);
        return true;
    });

    ctx.create_region(11, "RegionB");
    ctx.bind_resource_input(11, 2);
    ctx.bind_upstream_producer(11, 10);
    ctx.set_region_kernel(11, [](twrf::TemporalWorkRegion& self, twrf::LogicalStateStore& store,
                                 const std::vector<const twrf::VersionedResource*>& inputs) -> bool {
        size_t sz = 0;
        twrf::VersionNumber ver = 0;
        const uint8_t* raw_up = store.read_output(10, sz, ver);
        float up_val = (raw_up && sz >= sizeof(float)) ? *reinterpret_cast<const float*>(raw_up) : 0.0f;
        float in_val = inputs.empty() || !inputs[0] ? 0.0f : inputs[0]->get_value<float>(0.0f);
        float out_val = up_val + in_val;
        store.write_output(self.id(), &out_val, sizeof(float), self.current_output_version() + 1);
        return true;
    });

    ctx.compile_graph();

    // Frame 1
    ctx.submit_frame();

    // Frame 2: Static
    ctx.submit_frame();

    // Frame 3: Update ResAlpha
    ctx.update_resource_value(1, 100.0f);
    ctx.submit_frame();

    size_t out_sz = 0;
    twrf::VersionNumber out_v = 0;
    const uint8_t* raw = ctx.read_state(11, out_sz, out_v);
    float final_val = (raw && out_sz >= sizeof(float)) ? *reinterpret_cast<const float*>(raw) : 0.0f;

    return ApiScenarioOutput{
        ctx.trace().to_string(),
        ctx.metrics().total_twr_executions,
        ctx.metrics().total_twr_skips,
        final_val
    };
}

int main() {
    TWRF_TEST_START("S4-06: Architecture-Native API Command Stream Replay & Determinism");

    ApiScenarioOutput run1 = run_api_scenario();
    ApiScenarioOutput run2 = run_api_scenario();

    TWRF_ASSERT(run1.total_executions == run2.total_executions,
                "S4-06 Failed: Total execution count differs across identical API runs");
    TWRF_ASSERT(run1.total_skips == run2.total_skips,
                "S4-06 Failed: Total skip count differs across identical API runs");
    TWRF_ASSERT(std::abs(run1.state_val - run2.state_val) < 1e-6f,
                "S4-06 Failed: Final state store output differs across identical API runs");
    TWRF_ASSERT(run1.trace_string == run2.trace_string,
                "S4-06 Failed: Execution trace differs across identical API runs");

    // Expected final value: (100.0 * 2) + 10.0 = 210.0
    TWRF_ASSERT(std::abs(run1.state_val - 210.0f) < 1e-5f,
                "S4-06 Failed: Final computed state value must equal 210.0");

    TWRF_TEST_PASS("S4-06: Architecture-Native API Command Stream Replay & Determinism");
    return 0;
}
