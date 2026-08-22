find_package(GTest CONFIG REQUIRED)

add_executable(
    dxa_tests
    client_options_test.cpp
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
dxa_enable_project_warnings(dxa_tests)

include(GoogleTest)
gtest_discover_tests(dxa_tests)

