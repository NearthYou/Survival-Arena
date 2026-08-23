find_package(GTest CONFIG REQUIRED)

add_executable(
    dxa_tests
    client_options_test.cpp
    engine_benchmark_report_test.cpp
    engine_gpu_pass_timing_test.cpp
    engine_perspective_frustum_test.cpp
    engine_stress_scene_test.cpp
    engine_animation_playback_test.cpp
    engine_asset_file_test.cpp
    engine_frame_clock_test.cpp
    engine_input_state_test.cpp
    engine_resource_pool_test.cpp
    simulation_ai_decision_test.cpp
    simulation_behavior_tree_test.cpp
    simulation_combat_test.cpp
    simulation_math_test.cpp
    simulation_match_types_test.cpp
    simulation_nav_agent_test.cpp
    simulation_navmesh_test.cpp
    simulation_safe_zone_test.cpp
    simulation_benchmark_options_test.cpp
    simulation_spatial_index_test.cpp
    foundation_test.cpp
)
target_link_libraries(
    dxa_tests
    PRIVATE
        GTest::gtest_main
        dxa_client_options
        dxa_engine_core
        dxa_simulation
        dxa_simulation_benchmark_options
)

if(WIN32)
    find_program(DXA_POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)

    target_sources(
        dxa_tests
        PRIVATE
            asset_model_importer_test.cpp
            asset_tool_options_test.cpp
            asset_texture_cooker_test.cpp
            engine_app_test.cpp
            engine_asset_scene_renderer_test.cpp
            engine_graphics_config_test.cpp
            engine_gpu_frame_timer_test.cpp
            engine_hybrid_deferred_renderer_test.cpp
            engine_window_test.cpp
            navigation_ground_pick_test.cpp
    )
    target_link_libraries(
        dxa_tests
        PRIVATE
            dxa_asset_importer
            dxa_asset_tool_options
            dxa_engine
            dxa_navigation_demo_core
            dxa_texture_cooker
    )
    target_compile_definitions(
        dxa_tests
        PRIVATE
            DXA_TEST_SHADER_PATH="${PROJECT_SOURCE_DIR}/assets/shaders/forward.hlsl"
            DXA_TEST_ASSET_SHADER_PATH="${PROJECT_SOURCE_DIR}/assets/shaders/asset_scene.hlsl"
            DXA_TEST_SHADER_ROOT="${PROJECT_SOURCE_DIR}/assets/shaders"
            DXA_TEST_ASSET_ROOT="${PROJECT_SOURCE_DIR}/assets/runtime"
    )

    add_test(
        NAME BenchmarkRunner.GuardsEvidenceBoundary
        COMMAND
            ${DXA_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File "${PROJECT_SOURCE_DIR}/tests/benchmark_runner_test.ps1"
            -RepositoryRoot "${PROJECT_SOURCE_DIR}"
    )

    add_test(
        NAME BenchmarkComparison.ValidatesComparableRuns
        COMMAND
            ${DXA_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File "${PROJECT_SOURCE_DIR}/tests/benchmark_comparison_test.ps1"
            -RepositoryRoot "${PROJECT_SOURCE_DIR}"
    )

    add_test(
        NAME SimulationBenchmarkRunner.GuardsEvidenceBoundary
        COMMAND
            ${DXA_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File "${PROJECT_SOURCE_DIR}/tests/simulation_benchmark_runner_test.ps1"
            -RepositoryRoot "${PROJECT_SOURCE_DIR}"
    )
endif()

dxa_enable_project_warnings(dxa_tests)

include(GoogleTest)
gtest_discover_tests(dxa_tests)

