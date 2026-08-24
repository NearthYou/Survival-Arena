add_library(
    dxa_lobby_cli_support
    STATIC
        src/LobbyCliCommand.cpp
        src/LobbyCliOutput.cpp
)
target_include_directories(
    dxa_lobby_cli_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(dxa_lobby_cli_support PUBLIC dxa_protocol)
dxa_enable_project_warnings(dxa_lobby_cli_support)

add_executable(dxa_lobby_cli src/main.cpp)
target_link_libraries(
    dxa_lobby_cli
    PRIVATE
        dxa_lobby_cli_support
        dxa_lobby_client
)
dxa_enable_project_warnings(dxa_lobby_cli)
