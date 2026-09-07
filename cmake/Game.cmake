if(NOT DXA_BUILD_GAME)
    return()
endif()
set(game_source_root "${PROJECT_SOURCE_DIR}/game")
cmake_path(IS_PREFIX game_source_root "${PROJECT_BINARY_DIR}" NORMALIZE game_output_in_source)
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR OR game_output_in_source)
    message(FATAL_ERROR "Use a separate build directory outside the tracked game source tree")
endif()
if(NOT WIN32 OR NOT CMAKE_GENERATOR STREQUAL "Visual Studio 17 2022" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "dxa_game requires Visual Studio 2022, C++17 and x64")
endif()
set(DXA_GAME_CONFIGURATION Debug CACHE STRING "Game build configuration")
set_property(CACHE DXA_GAME_CONFIGURATION PROPERTY STRINGS Debug Release)
if(NOT DXA_GAME_CONFIGURATION MATCHES "^(Debug|Release)$")
    message(FATAL_ERROR "DXA_GAME_CONFIGURATION must be Debug or Release")
endif()
set(CMAKE_CONFIGURATION_TYPES "${DXA_GAME_CONFIGURATION}" CACHE STRING "Source port configuration" FORCE)
set(DXA_GAME_SDK_ROOT "$ENV{DXA_GAME_SDK_ROOT}" CACHE PATH "Independent pinned game SDK package")
set(DXA_GAME_ASSET_ROOT "$ENV{DXA_GAME_ASSET_ROOT}" CACHE PATH "Independent pinned game Resources package")
if(NOT DXA_GAME_SDK_ROOT OR NOT DXA_GAME_ASSET_ROOT)
    message(FATAL_ERROR "Set DXA_GAME_SDK_ROOT and DXA_GAME_ASSET_ROOT to the independent packages. Use DXA_BUILD_GAME=OFF for server/tool-only work.")
endif()
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
find_program(DXA_GAME_POWERSHELL NAMES pwsh REQUIRED)
set(DXA_GAME_BUILD_ROOT "${PROJECT_BINARY_DIR}/game/${DXA_GAME_CONFIGURATION}")
set(DXA_GAME_RUNTIME_ROOT "${DXA_GAME_BUILD_ROOT}/runtime")
set(DXA_GAME_EXECUTABLE "${DXA_GAME_RUNTIME_ROOT}/Binaries/dxa_game.exe")
set(game_lock "${PROJECT_SOURCE_DIR}/game/packages.lock.json")
if(DXA_GAME_CONFIGURATION STREQUAL "Release")
    set(game_lock "${PROJECT_SOURCE_DIR}/game/packages.release.lock.json")
endif()
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/game_packages.py" verify
        --sdk-root "${DXA_GAME_SDK_ROOT}" --asset-root "${DXA_GAME_ASSET_ROOT}" --lock "${game_lock}"
    RESULT_VARIABLE game_package_result OUTPUT_VARIABLE game_package_output ERROR_VARIABLE game_package_error
)
if(NOT game_package_result EQUAL 0)
    message(FATAL_ERROR "${game_package_error}")
endif()
message(STATUS "Verified independent game packages: ${game_package_output}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${game_lock}")
add_custom_target(dxa_game_content
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/stage_game_runtime.py"
        --repository "${PROJECT_SOURCE_DIR}" --build-root "${DXA_GAME_BUILD_ROOT}"
        --sdk-root "${DXA_GAME_SDK_ROOT}" --asset-root "${DXA_GAME_ASSET_ROOT}"
        --configuration "${DXA_GAME_CONFIGURATION}"
    COMMENT "Verify packages and stage independent game runtime"
    VERBATIM
)
# Keep the original MSBuild compilation settings and per-file PCH/FxCompile
# metadata. Both projects are repository sources, with all outputs in the build.
foreach(game_project IN ITEMS Engine Client)
    if(game_project STREQUAL "Engine")
        set(game_target dxa_game_engine)
        set(game_output "${DXA_GAME_BUILD_ROOT}/lib/dxa_game_engine.lib")
        set(game_dependency dxa_game_content)
    else()
        set(game_target dxa_game)
        set(game_output "${DXA_GAME_EXECUTABLE}")
        set(game_dependency dxa_game_engine)
    endif()
    add_custom_target(${game_target} ALL
        COMMAND "${CMAKE_VS_MSBUILD_COMMAND}" "${PROJECT_SOURCE_DIR}/game/${game_project}/${game_project}.vcxproj"
            /t:Build /m:1 /nodeReuse:false /v:minimal "/p:Configuration=${DXA_GAME_CONFIGURATION}" /p:Platform=x64
            /p:DxaGameProbe=
            "/p:DxaGameSdkRoot=${DXA_GAME_SDK_ROOT}" "/p:DxaGameBuildRoot=${DXA_GAME_BUILD_ROOT}"
            "/p:SolutionDir=${PROJECT_BINARY_DIR}/"
            "/flp:logfile=${DXA_GAME_BUILD_ROOT}/${game_project}-build.log;verbosity=normal"
            "/bl:${DXA_GAME_BUILD_ROOT}/${game_project}-build.binlog"
        DEPENDS ${game_dependency}
        BYPRODUCTS "${game_output}"
        COMMENT "Build repository game/${game_project} as ${game_target}"
        VERBATIM
    )
endforeach()
add_custom_command(TARGET dxa_game POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/write_game_build_receipt.py"
        --repository "${PROJECT_SOURCE_DIR}" --build-root "${DXA_GAME_BUILD_ROOT}" --sdk-root "${DXA_GAME_SDK_ROOT}"
        --configuration "${DXA_GAME_CONFIGURATION}"
    VERBATIM)
add_custom_target(play_game
    COMMAND "${DXA_GAME_POWERSHELL}" -NoProfile -File "${PROJECT_SOURCE_DIR}/scripts/run_game.ps1"
        -Executable "${DXA_GAME_EXECUTABLE}"
    DEPENDS dxa_game VERBATIM
)
include(GameTests)
