add_library(dxa_protocol INTERFACE)
target_include_directories(
    dxa_protocol
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
