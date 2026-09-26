cmake_minimum_required(VERSION 3.20)
project(twrf_simulator VERSION 0.1.0 LANGUAGES C CXX)

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
add_twrf_test(test_b3_parity tests/test_b3_parity.cpp)
add_twrf_test(test_oracle_matrix tests/test_oracle_matrix.cpp)
add_twrf_test(test_sensitivity_grid tests/test_sensitivity_grid.cpp)

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

# -----------------------------------------------------------------------------
# TWRF-DOOM: Persistent Temporal Work Region DOOM Integration
# -----------------------------------------------------------------------------
set(DOOM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/doomgeneric/doomgeneric)

set(DOOM_SOURCES
    ${DOOM_DIR}/dummy.c
    ${DOOM_DIR}/am_map.c
    ${DOOM_DIR}/doomdef.c
    ${DOOM_DIR}/doomstat.c
    ${DOOM_DIR}/dstrings.c
    ${DOOM_DIR}/d_event.c
    ${DOOM_DIR}/d_items.c
    ${DOOM_DIR}/d_iwad.c
    ${DOOM_DIR}/d_loop.c
    ${DOOM_DIR}/d_main.c
    ${DOOM_DIR}/d_mode.c
    ${DOOM_DIR}/d_net.c
    ${DOOM_DIR}/f_finale.c
    ${DOOM_DIR}/f_wipe.c
    ${DOOM_DIR}/g_game.c
    ${DOOM_DIR}/hu_lib.c
    ${DOOM_DIR}/hu_stuff.c
    ${DOOM_DIR}/info.c
    ${DOOM_DIR}/i_cdmus.c
    ${DOOM_DIR}/i_endoom.c
    ${DOOM_DIR}/i_joystick.c
    ${DOOM_DIR}/i_scale.c
    ${DOOM_DIR}/i_sound.c
    ${DOOM_DIR}/i_system.c
    ${DOOM_DIR}/i_timer.c
    ${DOOM_DIR}/memio.c
    ${DOOM_DIR}/m_argv.c
    ${DOOM_DIR}/m_bbox.c
    ${DOOM_DIR}/m_cheat.c
    ${DOOM_DIR}/m_config.c
    ${DOOM_DIR}/m_controls.c
    ${DOOM_DIR}/m_fixed.c
    ${DOOM_DIR}/m_menu.c
    ${DOOM_DIR}/m_misc.c
    ${DOOM_DIR}/m_random.c
    ${DOOM_DIR}/p_ceilng.c
    ${DOOM_DIR}/p_doors.c
    ${DOOM_DIR}/p_enemy.c
    ${DOOM_DIR}/p_floor.c
    ${DOOM_DIR}/p_inter.c
    ${DOOM_DIR}/p_lights.c
    ${DOOM_DIR}/p_map.c
    ${DOOM_DIR}/p_maputl.c
    ${DOOM_DIR}/p_mobj.c
    ${DOOM_DIR}/p_plats.c
    ${DOOM_DIR}/p_pspr.c
    ${DOOM_DIR}/p_saveg.c
    ${DOOM_DIR}/p_setup.c
    ${DOOM_DIR}/p_sight.c
    ${DOOM_DIR}/p_spec.c
    ${DOOM_DIR}/p_switch.c
    ${DOOM_DIR}/p_telept.c
    ${DOOM_DIR}/p_tick.c
    ${DOOM_DIR}/p_user.c
    ${DOOM_DIR}/r_bsp.c
    ${DOOM_DIR}/r_data.c
    ${DOOM_DIR}/r_draw.c
    ${DOOM_DIR}/r_main.c
    ${DOOM_DIR}/r_plane.c
    ${DOOM_DIR}/r_segs.c
    ${DOOM_DIR}/r_sky.c
    ${DOOM_DIR}/r_things.c
    ${DOOM_DIR}/sha1.c
    ${DOOM_DIR}/sounds.c
    ${DOOM_DIR}/statdump.c
    ${DOOM_DIR}/st_lib.c
    ${DOOM_DIR}/st_stuff.c
    ${DOOM_DIR}/s_sound.c
    ${DOOM_DIR}/tables.c
    ${DOOM_DIR}/v_video.c
    ${DOOM_DIR}/wi_stuff.c
    ${DOOM_DIR}/w_checksum.c
    ${DOOM_DIR}/w_file.c
    ${DOOM_DIR}/w_main.c
    ${DOOM_DIR}/w_wad.c
    ${DOOM_DIR}/z_zone.c
    ${DOOM_DIR}/w_file_stdc.c
    ${DOOM_DIR}/i_input.c
    ${DOOM_DIR}/i_video.c
    ${DOOM_DIR}/doomgeneric.c
)

# Silence third-party legacy C warnings for doomgeneric
if(NOT MSVC)
    set_source_files_properties(${DOOM_SOURCES} PROPERTIES COMPILE_FLAGS "-w -std=gnu89 -DNORMALUNIX -D_DEFAULT_SOURCE")
else()
    set_source_files_properties(${DOOM_SOURCES} PROPERTIES COMPILE_FLAGS "/w -D_CRT_SECURE_NO_WARNINGS")
endif()

add_executable(twrf_doom
    src/doom/doomgeneric_twrf.cpp
    ${DOOM_SOURCES}
)

target_include_directories(twrf_doom PRIVATE
    ${DOOM_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(twrf_doom PRIVATE twrf_core)

if(WIN32)
    target_link_libraries(twrf_doom PRIVATE winmm gdi32 user32)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(twrf_doom PRIVATE m)
endif()

