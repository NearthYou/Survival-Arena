if(WIN32)
    find_package(directxtk CONFIG REQUIRED)

    add_library(dxa_navigation_demo_core STATIC src/GroundPicking.cpp)
    target_include_directories(
        dxa_navigation_demo_core
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(
        dxa_navigation_demo_core
        PUBLIC
            dxa_engine_core
            dxa_simulation
        PRIVATE
            Microsoft::DirectXTK
    )
    dxa_enable_project_warnings(dxa_navigation_demo_core)

    add_executable(dxa_navigation_demo src/main.cpp)
    target_link_libraries(
        dxa_navigation_demo
        PRIVATE
            dxa_engine
            dxa_navigation_demo_core
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
