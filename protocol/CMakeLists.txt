add_library(
    dxa_protocol
    STATIC
        src/AsioFramedConnection.cpp
        src/ByteCodec.cpp
        src/LobbyMessageCodec.cpp
        src/TcpFrame.cpp
)
find_package(boost_asio CONFIG REQUIRED)
target_include_directories(
    dxa_protocol
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(dxa_protocol PUBLIC Boost::asio)
target_compile_definitions(dxa_protocol PUBLIC BOOST_ASIO_NO_DEPRECATED)
if(WIN32)
    target_compile_definitions(dxa_protocol PUBLIC _WIN32_WINNT=0x0A00)
endif()
dxa_enable_project_warnings(dxa_protocol)
