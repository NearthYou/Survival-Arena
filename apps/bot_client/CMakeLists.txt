add_library(
    dxa_bot_client_options
    STATIC
        src/BotClientOptions.cpp
)
target_include_directories(
    dxa_bot_client_options
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(dxa_bot_client_options PUBLIC dxa_protocol)
dxa_enable_project_warnings(dxa_bot_client_options)

add_executable(dxa_bot_client src/main.cpp)
target_link_libraries(
    dxa_bot_client
    PRIVATE
        dxa_bot_client_options
        dxa_lobby_client
)
dxa_enable_project_warnings(dxa_bot_client)
