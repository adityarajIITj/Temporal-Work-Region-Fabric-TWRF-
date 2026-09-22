#include "twrf/core/types.hpp"
#include "twrf/core/resource.hpp"
#include "twrf/core/state_store.hpp"
#include "twrf/core/twr.hpp"
#include "twrf/core/graph.hpp"
#include "twrf/core/change_tracker.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/trace.hpp"
#include "twrf/core/metrics.hpp"
#include "twrf/core/cost_model.hpp"

namespace twrf {

// Explicit anchor to ensure the translation unit compiles and links cleanly.
const char* get_twrf_version() {
    return "TWRF-v0.1-SubPlan1";
}

} // namespace twrf
