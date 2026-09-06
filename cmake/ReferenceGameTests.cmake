# Source-native regression tests stay independent of the experimental client.
if(NOT WIN32 OR NOT DXA_BUILD_TESTS)
    return()
endif()
find_program(DXA_REFERENCE_TEST_POWERSHELL NAMES pwsh powershell REQUIRED)

add_test(
    NAME ReferenceOracleBuilder.PreservesRevisionAndSceneFlow
    COMMAND
        ${DXA_REFERENCE_TEST_POWERSHELL}
        -NoProfile
        -File "${PROJECT_SOURCE_DIR}/tests/reference_oracle_builder_test.ps1"
        -RepositoryRoot "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(ReferenceOracleBuilder.PreservesRevisionAndSceneFlow PROPERTIES TIMEOUT 45)
add_test(
    NAME ReferenceOracleCapture.OwnsDeferredFileNames
    COMMAND
        ${DXA_REFERENCE_TEST_POWERSHELL}
        -NoProfile
        -File "${PROJECT_SOURCE_DIR}/tests/reference_frame_capture_test.ps1"
        -RepositoryRoot "${PROJECT_SOURCE_DIR}"
        -CMakeExecutable "${CMAKE_COMMAND}"
)
set_tests_properties(ReferenceOracleCapture.OwnsDeferredFileNames PROPERTIES TIMEOUT 45)
add_test(
    NAME ReferenceGameRunner.RequiresApprovedSealedRuntime
    COMMAND
        ${DXA_REFERENCE_TEST_POWERSHELL}
        -NoProfile
        -File "${PROJECT_SOURCE_DIR}/tests/reference_game_runner_test.ps1"
        -RepositoryRoot "${PROJECT_SOURCE_DIR}"
)
add_test(
    NAME ReferenceGameCMake.KeepsOutputImmutableAndBuildIncremental
    COMMAND
        ${DXA_REFERENCE_TEST_POWERSHELL}
        -NoProfile
        -File "${PROJECT_SOURCE_DIR}/tests/reference_game_cmake_test.ps1"
        -RepositoryRoot "${PROJECT_SOURCE_DIR}"
        -CMakeExecutable "${CMAKE_COMMAND}"
)
set_tests_properties(ReferenceGameCMake.KeepsOutputImmutableAndBuildIncremental PROPERTIES TIMEOUT 90)
add_test(
    NAME ReferenceGameCollisionDebug.HidesGeometryWithoutDisablingCollision
    COMMAND
        ${DXA_REFERENCE_TEST_POWERSHELL}
        -NoProfile
        -File "${PROJECT_SOURCE_DIR}/tests/reference_collision_debug_toggle_test.ps1"
        -RepositoryRoot "${PROJECT_SOURCE_DIR}"
        -CMakeExecutable "${CMAKE_COMMAND}"
)
set_tests_properties(ReferenceGameCollisionDebug.HidesGeometryWithoutDisablingCollision PROPERTIES TIMEOUT 45)
if(DXA_REFERENCE_GAME_DIRECTORY)
    add_test(
        NAME ReferenceGameSelection.PreservesCharacterAcrossRepeatedStart
        COMMAND
            ${DXA_REFERENCE_TEST_POWERSHELL}
            -NoProfile
            -File "${PROJECT_SOURCE_DIR}/tests/reference_character_selection_test.ps1"
            -RepositoryRoot "${PROJECT_SOURCE_DIR}"
            -SourceFile "${DXA_REFERENCE_GAME_DIRECTORY}/source-snapshot/Client/CharacterSelectScene.cpp"
            -CMakeExecutable "${CMAKE_COMMAND}"
            -ApplyPatch
            -ApplySkinPatch
    )
    set_tests_properties(ReferenceGameSelection.PreservesCharacterAcrossRepeatedStart PROPERTIES TIMEOUT 45)
    add_test(
        NAME ReferenceGamePicking.SynchronizesCameraBeforeIndexing
        COMMAND
            ${DXA_REFERENCE_TEST_POWERSHELL}
            -NoProfile
            -File "${PROJECT_SOURCE_DIR}/tests/reference_picking_matrix_test.ps1"
            -RepositoryRoot "${PROJECT_SOURCE_DIR}"
            -SourceFile "${DXA_REFERENCE_GAME_DIRECTORY}/source-snapshot/Engine/SceneObjectManager.cpp"
            -CMakeExecutable "${CMAKE_COMMAND}"
            -ApplyPatch
    )
    set_tests_properties(ReferenceGamePicking.SynchronizesCameraBeforeIndexing PROPERTIES TIMEOUT 45)
    add_test(
        NAME ReferenceGameAnimation.FollowsAcceptedPlayerActions
        COMMAND
            ${DXA_REFERENCE_TEST_POWERSHELL}
            -NoProfile
            -File "${PROJECT_SOURCE_DIR}/tests/reference_animation_sync_test.ps1"
            -RepositoryRoot "${PROJECT_SOURCE_DIR}"
            -SourceFile "${DXA_REFERENCE_GAME_DIRECTORY}/source-snapshot/Engine/AnimationStateMachine.cpp"
            -CMakeExecutable "${CMAKE_COMMAND}"
            -ApplyPatch
    )
    set_tests_properties(ReferenceGameAnimation.FollowsAcceptedPlayerActions PROPERTIES TIMEOUT 45)
    add_test(
        NAME ReferenceGameAnimation.KeepsLocomotionDuringCombatPursuit
        COMMAND
            ${DXA_REFERENCE_TEST_POWERSHELL}
            -NoProfile
            -File "${PROJECT_SOURCE_DIR}/tests/reference_animation_sync_test.ps1"
            -RepositoryRoot "${PROJECT_SOURCE_DIR}"
            -SourceFile "${DXA_REFERENCE_GAME_DIRECTORY}/source-snapshot/Engine/AnimationStateMachine.cpp"
            -CMakeExecutable "${CMAKE_COMMAND}"
            -ApplyPatch -ApplyCombatPatch -CheckCombatMovement
    )
    set_tests_properties(ReferenceGameAnimation.KeepsLocomotionDuringCombatPursuit PROPERTIES TIMEOUT 45)
    add_test(
        NAME ReferenceGameAnimation.DoesNotReplayCompletedActions
        COMMAND
            ${DXA_REFERENCE_TEST_POWERSHELL}
            -NoProfile
            -File "${PROJECT_SOURCE_DIR}/tests/reference_animation_sync_test.ps1"
            -RepositoryRoot "${PROJECT_SOURCE_DIR}"
            -SourceFile "${DXA_REFERENCE_GAME_DIRECTORY}/source-snapshot/Engine/AnimationStateMachine.cpp"
            -CMakeExecutable "${CMAKE_COMMAND}"
            -ApplyPatch -ApplyCombatPatch -ApplyEpisodePatch
            -CheckCombatMovement -CheckCompletedAction
    )
    set_tests_properties(ReferenceGameAnimation.DoesNotReplayCompletedActions PROPERTIES TIMEOUT 45)
    add_test(
        NAME ReferenceGameNavigation.PreservesWalkableCornerRoutes
        COMMAND
            ${DXA_REFERENCE_TEST_POWERSHELL}
            -NoProfile
            -File "${PROJECT_SOURCE_DIR}/tests/reference_navigation_path_test.ps1"
            -RepositoryRoot "${PROJECT_SOURCE_DIR}"
            -SourceFile "${DXA_REFERENCE_GAME_DIRECTORY}/source-snapshot/Engine/NavMesh.cpp"
            -CMakeExecutable "${CMAKE_COMMAND}"
            -ApplyPatch
    )
    set_tests_properties(ReferenceGameNavigation.PreservesWalkableCornerRoutes PROPERTIES TIMEOUT 45)
    add_test(
        NAME ReferenceGameAudio.SettingsControlFutureAndOverlappingVoices
        COMMAND
            ${DXA_REFERENCE_TEST_POWERSHELL}
            -NoProfile
            -File "${PROJECT_SOURCE_DIR}/tests/reference_audio_settings_test.ps1"
            -RepositoryRoot "${PROJECT_SOURCE_DIR}"
            -ReferenceRoot "${DXA_REFERENCE_GAME_DIRECTORY}"
            -CMakeExecutable "${CMAKE_COMMAND}"
            -ApplyPatch
    )
    set_tests_properties(ReferenceGameAudio.SettingsControlFutureAndOverlappingVoices PROPERTIES TIMEOUT 45)
    add_test(
        NAME ReferenceGameCraft.PreparesUpcomingStateWithoutTouchingCurrent
        COMMAND
            ${DXA_REFERENCE_TEST_POWERSHELL}
            -NoProfile
            -File "${PROJECT_SOURCE_DIR}/tests/reference_craft_preparation_test.ps1"
            -RepositoryRoot "${PROJECT_SOURCE_DIR}"
            -ReferenceRoot "${DXA_REFERENCE_GAME_DIRECTORY}/source-snapshot"
            -CMakeExecutable "${CMAKE_COMMAND}"
            -ApplyPatch
    )
    set_tests_properties(ReferenceGameCraft.PreparesUpcomingStateWithoutTouchingCurrent PROPERTIES TIMEOUT 45)
endif()
