add_library(
    dxa_engine_assets
    STATIC
        src/assets/AnimationPlayback.cpp
        src/assets/AssetFile.cpp
)
target_include_directories(
    dxa_engine_assets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
dxa_enable_project_warnings(dxa_engine_assets)

add_library(dxa_engine_core INTERFACE)
target_include_directories(
    dxa_engine_core
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(dxa_engine_core INTERFACE dxa_engine_assets)

if(WIN32)
    find_package(directxtk CONFIG REQUIRED)

    add_library(
        dxa_engine
        STATIC
            src/windows/AssetSceneRenderer.cpp
            src/windows/EngineApp.cpp
            src/windows/ForwardRenderer.cpp
            src/windows/GraphicsDevice.cpp
            src/windows/Window.cpp
    )
    target_include_directories(
        dxa_engine
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(
        dxa_engine
        PRIVATE
            NOMINMAX
            UNICODE
            WIN32_LEAN_AND_MEAN
            _UNICODE
    )
    target_link_libraries(
        dxa_engine
        PUBLIC
            dxa_engine_core
        PRIVATE
            d3d11
            d3dcompiler
            dxgi
            Microsoft::DirectXTK
    )
    dxa_enable_project_warnings(dxa_engine)
endif()
