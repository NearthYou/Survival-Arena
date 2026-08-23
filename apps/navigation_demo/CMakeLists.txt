if(WIN32)
    add_executable(dxa_navigation_demo src/main.cpp)
    target_link_libraries(
        dxa_navigation_demo
        PRIVATE
            dxa_engine
            dxa_simulation
    )
    target_compile_definitions(
        dxa_navigation_demo
        PRIVATE
            NOMINMAX
            UNICODE
            WIN32_LEAN_AND_MEAN
            _UNICODE
    )
    dxa_enable_project_warnings(dxa_navigation_demo)

    set(
        DXA_NAVIGATION_SHADERS
        hybrid_geometry.hlsl
        hybrid_lighting.hlsl
        hybrid_shadow.hlsl
        hybrid_transparent.hlsl
    )
    foreach(shader IN LISTS DXA_NAVIGATION_SHADERS)
        add_custom_command(
            TARGET dxa_navigation_demo
            POST_BUILD
            COMMAND
                ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:dxa_navigation_demo>/shaders"
            COMMAND
                ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/assets/shaders/${shader}"
                "$<TARGET_FILE_DIR:dxa_navigation_demo>/shaders/${shader}"
            VERBATIM
        )
    endforeach()
    add_custom_command(
        TARGET dxa_navigation_demo
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/assets/runtime"
            "$<TARGET_FILE_DIR:dxa_navigation_demo>/assets"
        VERBATIM
    )

    if(DXA_BUILD_TESTS)
        add_test(
            NAME NavigationDemo.WarpSmoke
            COMMAND
                dxa_navigation_demo
                --warp
                --hidden
                --frames 120
                --auto-destination 20 10
                --verify-render
        )
        set_tests_properties(NavigationDemo.WarpSmoke PROPERTIES TIMEOUT 120)
    endif()
endif()
