if(NOT DXA_BUILD_TESTS)
    return()
endif()
add_test(NAME GamePackages.RejectsMissingCorruptAndLinkedPackages
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/game_packages_test.py")
add_test(NAME GameRelease.RejectsDebugCompilation
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/game_release_test.py")
add_test(NAME GameRelease.MeasuresActualFrameAndMemorySamples
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/game_measurement_test.py")
add_test(NAME GameDiagnostics.RejectsInvalidComparisons
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/game_diagnostics_test.py")
add_test(NAME GameRelease.PackagesOnlySealedRuntime
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/game_distribution_test.py")
add_test(NAME GameRelease.PerformsFileIOWithAssertionsDisabled
    COMMAND "${DXA_GAME_POWERSHELL}" -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/game_file_io_test.ps1"
        -RepositoryRoot "${PROJECT_SOURCE_DIR}" -CMakeExecutable "${CMAKE_COMMAND}")
add_test(NAME GameProbe.RejectsStaleSuccess
    COMMAND "${DXA_GAME_POWERSHELL}" -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/game_probe_runner_test.ps1"
        -RepositoryRoot "${PROJECT_SOURCE_DIR}")
add_test(NAME GameRuntime.ValidatesFromUnrelatedWorkingDirectory
    COMMAND "${DXA_GAME_EXECUTABLE}" --validate-runtime)
set_tests_properties(GameRuntime.ValidatesFromUnrelatedWorkingDirectory PROPERTIES
    WORKING_DIRECTORY "$ENV{TEMP}" TIMEOUT 90)
add_test(NAME GameRuntime.RejectsCorruptionAndRelocates
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/game_runtime_test.py"
        --executable "${DXA_GAME_EXECUTABLE}" --output "${PROJECT_BINARY_DIR}/runtime-tests")
set_tests_properties(GameRuntime.RejectsCorruptionAndRelocates PROPERTIES TIMEOUT 180)
foreach(scenario IN ITEMS selection picking animation navigation)
    if(scenario STREQUAL "selection")
        set(test_script reference_character_selection_test.ps1)
        set(test_source Client/CharacterSelectScene.cpp)
    elseif(scenario STREQUAL "picking")
        set(test_script reference_picking_matrix_test.ps1)
        set(test_source Engine/SceneObjectManager.cpp)
    elseif(scenario STREQUAL "animation")
        set(test_script reference_animation_sync_test.ps1)
        set(test_source Engine/AnimationStateMachine.cpp)
    else()
        set(test_script reference_navigation_path_test.ps1)
        set(test_source Engine/NavMesh.cpp)
    endif()
    set(scenario_options)
    if(scenario STREQUAL "selection")
        list(APPEND scenario_options -CheckAppearance)
    endif()
    if(scenario STREQUAL "animation")
        list(APPEND scenario_options -ImportedSource -CheckCombatMovement -CheckCompletedAction)
    endif()
    add_test(NAME GameSource.${scenario}
        COMMAND "${DXA_GAME_POWERSHELL}" -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/${test_script}"
            -RepositoryRoot "${PROJECT_SOURCE_DIR}" -SourceFile "${PROJECT_SOURCE_DIR}/game/${test_source}"
            -CMakeExecutable "${CMAKE_COMMAND}" ${scenario_options})
    set_tests_properties(GameSource.${scenario} PROPERTIES TIMEOUT 90)
endforeach()
add_test(NAME GameSource.CraftPreparation
    COMMAND "${DXA_GAME_POWERSHELL}" -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/reference_craft_preparation_test.ps1"
        -RepositoryRoot "${PROJECT_SOURCE_DIR}" -ReferenceRoot "${PROJECT_SOURCE_DIR}/game"
        -CMakeExecutable "${CMAKE_COMMAND}")
set_tests_properties(GameSource.CraftPreparation PROPERTIES TIMEOUT 90)
add_test(NAME GameSource.AudioSettings
    COMMAND "${DXA_GAME_POWERSHELL}" -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/reference_audio_settings_test.ps1"
        -RepositoryRoot "${PROJECT_SOURCE_DIR}" -ReferenceRoot "${PROJECT_SOURCE_DIR}/game"
        -SdkRoot "${DXA_GAME_SDK_ROOT}" -AssetRoot "${DXA_GAME_ASSET_ROOT}"
        -CMakeExecutable "${CMAKE_COMMAND}")
set_tests_properties(GameSource.AudioSettings PROPERTIES TIMEOUT 90)
add_test(NAME GameSource.CollisionDisplay
    COMMAND "${DXA_GAME_POWERSHELL}" -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/reference_collision_debug_toggle_test.ps1"
        -RepositoryRoot "${PROJECT_SOURCE_DIR}" -SourceFile "${PROJECT_SOURCE_DIR}/game/Client/LumiaIsland.cpp"
        -CMakeExecutable "${CMAKE_COMMAND}")
set_tests_properties(GameSource.CollisionDisplay PROPERTIES TIMEOUT 90)
add_test(NAME GameSource.SkillCooldownUI
    COMMAND "${DXA_GAME_POWERSHELL}" -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/game_skill_cooldown_ui_test.ps1"
        -RepositoryRoot "${PROJECT_SOURCE_DIR}" -CMakeExecutable "${CMAKE_COMMAND}")
set_tests_properties(GameSource.SkillCooldownUI PROPERTIES TIMEOUT 90)
foreach(mode IN ITEMS Admission Progression Runtime Collision)
    add_test(NAME GameSource.Skill${mode}
        COMMAND "${DXA_GAME_POWERSHELL}" -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/game_skill_progression_test.ps1"
            -RepositoryRoot "${PROJECT_SOURCE_DIR}" -CMakeExecutable "${CMAKE_COMMAND}" -Mode "${mode}")
    set_tests_properties(GameSource.Skill${mode} PROPERTIES TIMEOUT 90)
endforeach()
