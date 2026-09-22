#include "twrf/core/state_store.hpp"
#include "tests/test_common.hpp"

int main() {
    TWRF_TEST_START("S1-09: Logical State Store Accounting and Capacity Boundaries");

    // Initialize State Store with a 1024 byte capacity limit
    twrf::LogicalStateStore store(1024);

    int val1 = 12345;
    bool ok1 = store.write_output(1, &val1, sizeof(val1), 1, 1);
    TWRF_ASSERT(ok1, "Failed to write 4 bytes to slot 1");

    auto metrics = store.metrics();
    TWRF_ASSERT(metrics.current_allocated_bytes == sizeof(int), "Current allocated bytes incorrect");
    TWRF_ASSERT(metrics.peak_allocated_bytes == sizeof(int), "Peak allocated bytes incorrect");
    TWRF_ASSERT(metrics.total_writes == 1, "Write count should be 1");
    TWRF_ASSERT(metrics.total_write_bytes == sizeof(int), "Write bytes should match sizeof(int)");

    // Read back
    size_t sz = 0;
    twrf::VersionNumber ver = 0;
    const uint8_t* data = store.read_output(1, sz, ver);
    TWRF_ASSERT(data != nullptr && sz == sizeof(int) && ver == 1, "Read failed");
    TWRF_ASSERT(*reinterpret_cast<const int*>(data) == val1, "Read data mismatch");

    metrics = store.metrics();
    TWRF_ASSERT(metrics.total_reads == 1, "Read count should be 1");
    TWRF_ASSERT(metrics.total_read_bytes == sizeof(int), "Read bytes should match sizeof(int)");

    // Test capacity limit rejection: try to write a buffer larger than capacity (e.g. 2048 bytes)
    std::vector<uint8_t> large_buffer(2048, 0xFF);
    bool ok_overflow = store.write_output(2, large_buffer.data(), large_buffer.size(), 1, 2);
    TWRF_ASSERT(!ok_overflow, "State store must reject writes that exceed configured capacity limit");

    // Current allocated bytes must remain unchanged after rejected write
    metrics = store.metrics();
    TWRF_ASSERT(metrics.current_allocated_bytes == sizeof(int), "Allocated bytes corrupted after overflow attempt");

    TWRF_TEST_PASS("S1-09: Logical State Store Accounting and Capacity Boundaries");
    return 0;
}
