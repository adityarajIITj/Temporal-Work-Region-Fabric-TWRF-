cmake_minimum_required(VERSION 3.20)
project(twrf_simulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core static library
add_library(twrf_core STATIC
    src/core/twrf_core.cpp
)

target_include_directories(twrf_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Automated testing with CTest
enable_testing()

function(add_twrf_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE twrf_core)
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_twrf_test(test_twr_lifecycle tests/test_twr_lifecycle.cpp)
add_twrf_test(test_change_detection tests/test_change_detection.cpp)
add_twrf_test(test_dependency_propagation tests/test_dependency_propagation.cpp)
add_twrf_test(test_scheduler_determinism tests/test_scheduler_determinism.cpp)
add_twrf_test(test_cost_model tests/test_cost_model.cpp)
add_twrf_test(test_state_store tests/test_state_store.cpp)
add_twrf_test(test_multi_frame tests/test_multi_frame.cpp)
add_twrf_test(test_failure_propagation tests/test_failure_propagation.cpp)
add_twrf_test(test_state_commit_failure tests/test_state_commit_failure.cpp)
add_twrf_test(test_direct_dependency tests/test_direct_dependency.cpp)
add_twrf_test(test_dependency_audit tests/test_dependency_audit.cpp)
add_twrf_test(test_ray_camera_refresh tests/test_ray_camera_refresh.cpp)
add_twrf_test(test_producer_dependency_audit tests/test_producer_dependency_audit.cpp)

# Phase 2 (Sub-Plan 2): Functional Rasterizer & Reuse Tests
add_twrf_test(test_raster_reuse tests/test_raster_reuse.cpp)
add_twrf_test(test_object_transform tests/test_object_transform.cpp)
add_twrf_test(test_camera_change tests/test_camera_change.cpp)
add_twrf_test(test_texture_update tests/test_texture_update.cpp)
add_twrf_test(test_dynamic_scene tests/test_dynamic_scene.cpp)
add_twrf_test(test_tile_sweep tests/test_tile_sweep.cpp)
add_twrf_test(test_raster_repeatability tests/test_raster_repeatability.cpp)

# Phase 3 (Sub-Plan 3): Timing Model, Baselines & Break-Even Sweeps
add_twrf_test(test_model_accounting tests/test_model_accounting.cpp)
add_twrf_test(test_baseline_parity tests/test_baseline_parity.cpp)
add_twrf_test(test_change_rate_sweep tests/test_change_rate_sweep.cpp)
add_twrf_test(test_break_even_sanity tests/test_break_even_sanity.cpp)
add_twrf_test(test_worst_case tests/test_worst_case.cpp)
add_twrf_test(test_sweep_reproducibility tests/test_sweep_reproducibility.cpp)

# Phase 4 (Sub-Plan 4): Ray + Neural Workloads, Cross-Workload DAG & Native API
add_twrf_test(test_ray_batch_reuse tests/test_ray_batch_reuse.cpp)
add_twrf_test(test_neural_inference tests/test_neural_inference.cpp)
add_twrf_test(test_temporal_neural_state tests/test_temporal_neural_state.cpp)
add_twrf_test(test_cross_workload_dag tests/test_cross_workload_dag.cpp)
add_twrf_test(test_heterogeneous_fallback tests/test_heterogeneous_fallback.cpp)
add_twrf_test(test_api_replay tests/test_api_replay.cpp)

# Main comprehensive demonstration executable
add_executable(twrf_demo src/main.cpp)
target_link_libraries(twrf_demo PRIVATE twrf_core)
target_include_directories(twrf_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)


