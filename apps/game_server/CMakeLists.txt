add_library(
    dxa_game_server_core
    STATIC
        src/AuthoritativeMatch.cpp
        src/FixedTickScheduler.cpp
        src/GameServer.cpp
        src/GameServerOptions.cpp
        src/GameTicketStore.cpp
        src/ParticipantRoster.cpp
        src/SecureUdpTokenSource.cpp
)
find_package(spdlog CONFIG REQUIRED)
target_include_directories(
    dxa_game_server_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    dxa_game_server_core
    PUBLIC
        dxa_game_common
        dxa_protocol
        dxa_simulation
        spdlog::spdlog
)
if(WIN32)
    target_link_libraries(dxa_game_server_core PRIVATE bcrypt)
endif()
dxa_enable_project_warnings(dxa_game_server_core)

add_executable(dxa_game_server src/main.cpp)
target_link_libraries(dxa_game_server PRIVATE dxa_game_server_core)
dxa_enable_project_warnings(dxa_game_server)
