add_library(dxa_client_options INTERFACE)
target_include_directories(
    dxa_client_options
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

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
        )
        set_tests_properties(Client.WarpSmoke PROPERTIES TIMEOUT 15)
    endif()
endif()
