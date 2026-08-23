add_library(dxa_offline_match_benchmark_options INTERFACE)
target_include_directories(
    dxa_offline_match_benchmark_options
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(dxa_offline_match_benchmark src/main.cpp)
target_link_libraries(
    dxa_offline_match_benchmark
    PRIVATE
        dxa_offline_match_benchmark_options
        dxa_simulation
)
dxa_enable_project_warnings(dxa_offline_match_benchmark)
