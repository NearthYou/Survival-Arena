find_package(GTest CONFIG REQUIRED)

add_executable(
    dxa_tests
    client_options_test.cpp
    engine_asset_file_test.cpp
    engine_frame_clock_test.cpp
    engine_input_state_test.cpp
    engine_resource_pool_test.cpp
    foundation_test.cpp
)
target_link_libraries(
    dxa_tests
    PRIVATE
        GTest::gtest_main
        dxa_client_options
        dxa_engine_core
)

if(WIN32)
    target_sources(
        dxa_tests
        PRIVATE
            asset_model_importer_test.cpp
            asset_tool_options_test.cpp
            asset_texture_cooker_test.cpp
            engine_app_test.cpp
            engine_graphics_config_test.cpp
            engine_window_test.cpp
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
    )
endif()

dxa_enable_project_warnings(dxa_tests)

include(GoogleTest)
gtest_discover_tests(dxa_tests)

