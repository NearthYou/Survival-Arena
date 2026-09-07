cmake_minimum_required(VERSION 3.25)

project(
    SurvivalArenaOffline
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
option(DXA_BUILD_GAME "Build the repository Client and Engine game" ${WIN32})
if(WIN32)
    set(dxa_support_default OFF)
else()
    set(dxa_support_default ON)
endif()
option(DXA_BUILD_SUPPORT_TARGETS "Build the preserved server, protocol and benchmark targets" ${dxa_support_default})
option(DXA_BUILD_LEGACY_CLIENT "Build the experimental renderer and demo applications" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

if(DXA_BUILD_TESTS)
    include(CTest)
    enable_testing()
endif()

include(Game)

if(DXA_BUILD_SUPPORT_TARGETS)
    add_subdirectory(protocol)
    add_subdirectory(simulation)
    add_subdirectory(apps/game_common)
    add_subdirectory(apps/game_client)
    add_subdirectory(apps/game_server)
    add_subdirectory(engine)
    add_subdirectory(apps/lobby_server)
    add_subdirectory(apps/lobby_client)
    add_subdirectory(apps/lobby_cli)
    add_subdirectory(apps/bot_client)
    add_subdirectory(apps/client)
    if(DXA_BUILD_LEGACY_CLIENT)
        add_subdirectory(apps/navigation_demo)
        add_subdirectory(apps/offline_match_demo)
    endif()
    add_subdirectory(apps/offline_match_benchmark)
    add_subdirectory(apps/simulation_benchmark)
    add_subdirectory(apps/asset_tool)
endif()
include(ReferenceGame)

if(DXA_BUILD_TESTS)
    if(DXA_BUILD_SUPPORT_TARGETS)
        add_subdirectory(tests)
    endif()
    include(ReferenceGameTests)
endif()

