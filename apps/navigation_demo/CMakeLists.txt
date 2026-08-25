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

    include(DeployHybridRuntime)
    dxa_deploy_hybrid_runtime(dxa_navigation_demo)

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
