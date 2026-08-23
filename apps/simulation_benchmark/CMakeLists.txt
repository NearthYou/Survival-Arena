add_library(dxa_simulation_benchmark_options INTERFACE)
target_include_directories(
    dxa_simulation_benchmark_options
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(dxa_simulation_benchmark src/main.cpp)
target_link_libraries(
    dxa_simulation_benchmark
    PRIVATE
        dxa_simulation
        dxa_simulation_benchmark_options
)
dxa_enable_project_warnings(dxa_simulation_benchmark)
