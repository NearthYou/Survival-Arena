find_package(GTest CONFIG REQUIRED)

add_executable(
    dxa_tests
    client_options_test.cpp
    client_lobby_host_flow_test.cpp
    bot_client_options_test.cpp
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
    simulation_arena_map_test.cpp
    simulation_behavior_tree_test.cpp
    simulation_combat_test.cpp
    simulation_loot_test.cpp
    simulation_math_test.cpp
    simulation_match_types_test.cpp
    simulation_match_resolution_test.cpp
    simulation_offline_match_test.cpp
    simulation_offline_match_bot_test.cpp
    simulation_nav_agent_test.cpp
    simulation_navmesh_test.cpp
    simulation_safe_zone_test.cpp
    simulation_benchmark_options_test.cpp
    simulation_spatial_index_test.cpp
    foundation_test.cpp
    game_authoritative_match_test.cpp
    game_client_predictor_test.cpp
    game_fixed_tick_scheduler_test.cpp
    game_participant_roster_test.cpp
    game_server_adapter_test.cpp
    game_server_integration_test.cpp
    game_server_options_test.cpp
    game_session_integration_test.cpp
    game_snapshot_reassembler_test.cpp
    game_remote_interpolator_test.cpp
    game_ticket_store_test.cpp
    offline_match_benchmark_options_test.cpp
    protocol_ids_test.cpp
    protocol_game_types_test.cpp
    protocol_worker_control_codec_test.cpp
    protocol_game_tcp_codec_test.cpp
    protocol_game_udp_codec_test.cpp
    protocol_game_snapshot_codec_test.cpp
    protocol_byte_codec_test.cpp
    protocol_tcp_frame_test.cpp
    protocol_asio_framed_connection_test.cpp
    protocol_lobby_message_codec_test.cpp
    lobby_room_test.cpp
    lobby_cli_command_test.cpp
    lobby_cli_output_test.cpp
    lobby_server_options_test.cpp
    lobby_service_test.cpp
    lobby_tcp_integration_test.cpp
    lobby_ticket_test.cpp
    lobby_worker_control_integration_test.cpp
    lobby_worker_registry_test.cpp
)
target_link_libraries(
    dxa_tests
    PRIVATE
        GTest::gtest_main
        dxa_bot_client_options
        dxa_client_options
        dxa_client_network
        dxa_engine_core
        dxa_game_server_core
        dxa_game_client
        dxa_lobby_client
        dxa_lobby_cli_support
        dxa_lobby_core
        dxa_lobby_server_lib
        dxa_offline_match_benchmark_options
        dxa_protocol
        dxa_game_common
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

    add_test(
        NAME OfflineMatchBenchmarkRunner.GuardsEvidenceBoundary
        COMMAND
            ${DXA_POWERSHELL_EXECUTABLE}
            -NoProfile
            -File "${PROJECT_SOURCE_DIR}/tests/offline_match_benchmark_runner_test.ps1"
            -RepositoryRoot "${PROJECT_SOURCE_DIR}"
    )
endif()

dxa_enable_project_warnings(dxa_tests)

include(GoogleTest)
gtest_discover_tests(dxa_tests)

