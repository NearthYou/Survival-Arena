add_library(dxa_client_options INTERFACE)
target_include_directories(
    dxa_client_options
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(dxa_client_options INTERFACE dxa_engine_core)

if(WIN32)
    find_package(spdlog CONFIG REQUIRED)

    add_executable(dxa_client src/main.cpp)
    target_link_libraries(
        dxa_client
        PRIVATE
            dxa_client_options
            dxa_engine
            spdlog::spdlog
    )
    dxa_enable_project_warnings(dxa_client)

    add_custom_command(
        TARGET dxa_client
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:dxa_client>/shaders"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/assets/shaders/forward.hlsl"
            "$<TARGET_FILE_DIR:dxa_client>/shaders/forward.hlsl"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/assets/shaders/asset_scene.hlsl"
            "$<TARGET_FILE_DIR:dxa_client>/shaders/asset_scene.hlsl"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/assets/shaders/hybrid_geometry.hlsl"
            "$<TARGET_FILE_DIR:dxa_client>/shaders/hybrid_geometry.hlsl"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/assets/shaders/hybrid_lighting.hlsl"
            "$<TARGET_FILE_DIR:dxa_client>/shaders/hybrid_lighting.hlsl"
        COMMAND
            ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/assets/runtime"
            "$<TARGET_FILE_DIR:dxa_client>/assets"
        VERBATIM
    )

    if(DXA_BUILD_TESTS)
        add_test(
            NAME Client.WarpSmoke
            COMMAND
                dxa_client
                --warp
                --hidden
                --no-vsync
                --frames 3
                --width 320
                --height 180
                --verify-render
                --verify-asset-scene
        )
        set_tests_properties(Client.WarpSmoke PROPERTIES TIMEOUT 15)

        add_test(
            NAME Client.ShaderDeployment
            COMMAND
                ${CMAKE_COMMAND}
                "-DSOURCE_FILE=${PROJECT_SOURCE_DIR}/assets/shaders/forward.hlsl"
                "-DDEPLOYED_FILE=$<TARGET_FILE_DIR:dxa_client>/shaders/forward.hlsl"
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyFileMatch.cmake"
        )

        add_test(
            NAME Client.AssetShaderDeployment
            COMMAND
                ${CMAKE_COMMAND}
                "-DSOURCE_FILE=${PROJECT_SOURCE_DIR}/assets/shaders/asset_scene.hlsl"
                "-DDEPLOYED_FILE=$<TARGET_FILE_DIR:dxa_client>/shaders/asset_scene.hlsl"
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyFileMatch.cmake"
        )

        add_test(
            NAME Client.HybridGeometryShaderDeployment
            COMMAND
                ${CMAKE_COMMAND}
                "-DSOURCE_FILE=${PROJECT_SOURCE_DIR}/assets/shaders/hybrid_geometry.hlsl"
                "-DDEPLOYED_FILE=$<TARGET_FILE_DIR:dxa_client>/shaders/hybrid_geometry.hlsl"
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyFileMatch.cmake"
        )

        add_test(
            NAME Client.HybridLightingShaderDeployment
            COMMAND
                ${CMAKE_COMMAND}
                "-DSOURCE_FILE=${PROJECT_SOURCE_DIR}/assets/shaders/hybrid_lighting.hlsl"
                "-DDEPLOYED_FILE=$<TARGET_FILE_DIR:dxa_client>/shaders/hybrid_lighting.hlsl"
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyFileMatch.cmake"
        )

        add_test(
            NAME Client.CharacterAssetDeployment
            COMMAND
                ${CMAKE_COMMAND}
                "-DSOURCE_FILE=${PROJECT_SOURCE_DIR}/assets/runtime/characters/cyber-runner.dxam"
                "-DDEPLOYED_FILE=$<TARGET_FILE_DIR:dxa_client>/assets/characters/cyber-runner.dxam"
                -DMINIMUM_SIZE=1000000
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyRuntimeAsset.cmake"
        )

        add_test(
            NAME Client.FloorAssetDeployment
            COMMAND
                ${CMAKE_COMMAND}
                "-DSOURCE_FILE=${PROJECT_SOURCE_DIR}/assets/runtime/environment/prototype-floor.dxam"
                "-DDEPLOYED_FILE=$<TARGET_FILE_DIR:dxa_client>/assets/environment/prototype-floor.dxam"
                -DMINIMUM_SIZE=1000
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyRuntimeAsset.cmake"
        )

        add_test(
            NAME Client.TextureAssetDeployment
            COMMAND
                ${CMAKE_COMMAND}
                "-DSOURCE_FILE=${PROJECT_SOURCE_DIR}/assets/runtime/environment/colormap.dds"
                "-DDEPLOYED_FILE=$<TARGET_FILE_DIR:dxa_client>/assets/environment/colormap.dds"
                -DMINIMUM_SIZE=300000
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyRuntimeAsset.cmake"
        )
    endif()
endif()
