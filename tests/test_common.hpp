#pragma once

#include <iostream>
#include <cstdlib>
#include <string>

#define TWRF_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] Assertion failed: (" #cond ") at " \
                      << __FILE__ << ":" << __LINE__ << "\n" \
                      << "       Reason: " << (msg) << "\n"; \
            std::exit(1); \
        } \
    } while (0)

#define TWRF_TEST_START(name) \
    std::cout << "[RUN ] " << (name) << "...\n"

#define TWRF_TEST_PASS(name) \
    std::cout << "[PASS] " << (name) << " completed successfully.\n"
