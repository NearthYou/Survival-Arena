if(WIN32)
    add_executable(dxa_offline_match_demo src/main.cpp)
    target_link_libraries(
        dxa_offline_match_demo
        PRIVATE
            dxa_engine
            dxa_navigation_demo_core
            dxa_simulation
    )
    target_compile_definitions(
        dxa_offline_match_demo
        PRIVATE
            NOMINMAX
            UNICODE
            WIN32_LEAN_AND_MEAN
            _UNICODE
    )
    dxa_enable_project_warnings(dxa_offline_match_demo)

    include(DeployHybridRuntime)
    dxa_deploy_hybrid_runtime(dxa_offline_match_demo)

    if(DXA_BUILD_TESTS)
        add_test(
            NAME OfflineMatchDemo.WarpSmoke
            COMMAND
                dxa_offline_match_demo
                --warp
                --hidden
                --auto-match
                --verify-match
        )
        set_tests_properties(OfflineMatchDemo.WarpSmoke PROPERTIES TIMEOUT 120)
    endif()
endif()
