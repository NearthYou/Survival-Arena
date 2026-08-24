add_library(
    dxa_lobby_client
    STATIC
        src/LobbyClient.cpp
)
target_include_directories(
    dxa_lobby_client
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(dxa_lobby_client PUBLIC dxa_protocol)
dxa_enable_project_warnings(dxa_lobby_client)
