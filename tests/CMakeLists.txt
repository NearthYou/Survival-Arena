find_package(GTest CONFIG REQUIRED)

add_executable(dxa_tests foundation_test.cpp)
target_link_libraries(dxa_tests PRIVATE GTest::gtest_main)
dxa_enable_project_warnings(dxa_tests)

include(GoogleTest)
gtest_discover_tests(dxa_tests)

