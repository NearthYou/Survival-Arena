function(dxa_deploy_hybrid_runtime target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "unknown hybrid runtime target: ${target}")
    endif()

    set(
        dxa_hybrid_shaders
        hybrid_geometry.hlsl
        hybrid_lighting.hlsl
        hybrid_shadow.hlsl
        hybrid_transparent.hlsl
    )
    foreach(shader IN LISTS dxa_hybrid_shaders)
        add_custom_command(
            TARGET ${target}
            POST_BUILD
            COMMAND
                ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target}>/shaders"
            COMMAND
                ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/assets/shaders/${shader}"
                "$<TARGET_FILE_DIR:${target}>/shaders/${shader}"
            VERBATIM
        )
    endforeach()
    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/assets/runtime"
            "$<TARGET_FILE_DIR:${target}>/assets"
        VERBATIM
    )
endfunction()
