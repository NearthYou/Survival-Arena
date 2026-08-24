add_library(
    dxa_lobby_core
    STATIC
        src/LobbyService.cpp
        src/MatchTicketRegistry.cpp
        src/Room.cpp
        src/SecureTicketSource.cpp
        src/WorkerRegistry.cpp
)
target_include_directories(
    dxa_lobby_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    dxa_lobby_core
    PUBLIC
        dxa_protocol
)
if(WIN32)
    target_link_libraries(dxa_lobby_core PRIVATE bcrypt)
endif()
dxa_enable_project_warnings(dxa_lobby_core)

find_package(spdlog CONFIG REQUIRED)

add_library(
    dxa_lobby_server_lib
    STATIC
        src/LobbyServerOptions.cpp
        src/LobbyTcpServer.cpp
)
target_include_directories(
    dxa_lobby_server_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    dxa_lobby_server_lib
    PUBLIC
        dxa_lobby_core
        spdlog::spdlog
)
dxa_enable_project_warnings(dxa_lobby_server_lib)

add_executable(dxa_lobby_server src/main.cpp)
target_link_libraries(dxa_lobby_server PRIVATE dxa_lobby_server_lib)
dxa_enable_project_warnings(dxa_lobby_server)
