cmake_minimum_required(VERSION 3.25)

project(
    DX11SurvivalArena
    VERSION 0.1.0
    DESCRIPTION "DirectX 11 survival arena portfolio"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DXA_BUILD_TESTS "Build automated tests" ON)
option(DXA_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

if(DXA_BUILD_TESTS)
    include(CTest)
    enable_testing()
endif()

add_subdirectory(protocol)
add_subdirectory(simulation)
add_subdirectory(engine)
add_subdirectory(apps/lobby_server)
add_subdirectory(apps/client)
add_subdirectory(apps/navigation_demo)
add_subdirectory(apps/offline_match_demo)
add_subdirectory(apps/offline_match_benchmark)
add_subdirectory(apps/simulation_benchmark)
add_subdirectory(apps/asset_tool)

if(DXA_BUILD_TESTS)
    add_subdirectory(tests)
endif()

