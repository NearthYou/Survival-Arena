# Optional external comparison oracle. Never a default or game dependency.
option(DXA_BUILD_REFERENCE_ORACLE "Enable the external comparison oracle targets" OFF)
if(NOT DXA_BUILD_REFERENCE_ORACLE)
    return()
endif()
set(DXA_REFERENCE_SOURCE_ROOT "" CACHE PATH "Local reference source checkout")
set(DXA_REFERENCE_RUNTIME_ROOT "" CACHE PATH "External directory for private game builds")
if(NOT DXA_REFERENCE_SOURCE_ROOT AND NOT DXA_REFERENCE_RUNTIME_ROOT)
    return()
endif()
if(NOT WIN32 OR NOT DXA_REFERENCE_SOURCE_ROOT OR NOT DXA_REFERENCE_RUNTIME_ROOT)
    message(FATAL_ERROR "Source-native gameplay requires Windows and both reference source/runtime roots")
endif()
find_program(DXA_REFERENCE_POWERSHELL NAMES pwsh REQUIRED)
set(reference_revision "01b820a3ebcfd473a898dec1f5bc67c4ac77261e")
set(reference_builder "${PROJECT_SOURCE_DIR}/scripts/build_reference_oracle.ps1")
set(reference_runner "${PROJECT_SOURCE_DIR}/scripts/run_reference_game.ps1")
set(reference_preparer "${PROJECT_SOURCE_DIR}/scripts/prepare_reference_game.ps1")
set(reference_patches "${PROJECT_SOURCE_DIR}/scripts/reference_game_patches.ps1")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${reference_builder}" "${reference_patches}" "${reference_preparer}")
file(SHA256 "${reference_builder}" reference_builder_hash)
file(SHA256 "${reference_patches}" reference_patches_hash)
file(SHA256 "${reference_preparer}" reference_preparer_hash)
string(SHA256 reference_build_hash
    "${reference_revision};${reference_builder_hash};${reference_patches_hash};${reference_preparer_hash};${DXA_REFERENCE_SOURCE_ROOT};${DXA_REFERENCE_RUNTIME_ROOT};Debug;x64")
string(SUBSTRING "${reference_build_hash}" 0 16 reference_build_key)
set(DXA_REFERENCE_GAME_DIRECTORY "${DXA_REFERENCE_RUNTIME_ROOT}/game-${reference_build_key}"
    CACHE INTERNAL "Selected immutable source-native game build" FORCE)
set(reference_stamp "${PROJECT_BINARY_DIR}/reference-game-${reference_build_key}.stamp")
add_custom_command(
    # MSBuild precreates custom-output parents. Keep its stamp out of the
    # immutable runtime directory, which the builder must create itself.
    OUTPUT "${reference_stamp}"
    COMMAND "${DXA_REFERENCE_POWERSHELL}" -NoProfile -File "${reference_preparer}"
        -SourceRoot "${DXA_REFERENCE_SOURCE_ROOT}" -SourceRevision "${reference_revision}"
        -OutputRoot "${DXA_REFERENCE_GAME_DIRECTORY}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${reference_stamp}"
    DEPENDS "${reference_builder}" "${reference_patches}" "${reference_preparer}" "${reference_runner}"
    COMMENT "Build complete original Client and Engine for source-native gameplay"
    VERBATIM
)
add_custom_target(dxa_reference_game
    DEPENDS "${reference_stamp}"
    COMMAND "${DXA_REFERENCE_POWERSHELL}" -NoProfile -File "${reference_runner}"
        -RuntimeRoot "${DXA_REFERENCE_GAME_DIRECTORY}" -ValidateOnly
    VERBATIM
)
add_custom_target(play_reference_game
    COMMAND "${DXA_REFERENCE_POWERSHELL}" -NoProfile -File "${reference_runner}"
        -RuntimeRoot "${DXA_REFERENCE_GAME_DIRECTORY}" -Show
    DEPENDS dxa_reference_game
    VERBATIM
)
message(STATUS "Source-native gameplay: ${DXA_REFERENCE_GAME_DIRECTORY}")
