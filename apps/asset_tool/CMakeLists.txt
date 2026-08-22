if(WIN32)
    find_package(assimp CONFIG REQUIRED)
    find_package(directxtex CONFIG REQUIRED)

    add_library(dxa_asset_tool_options INTERFACE)
    target_include_directories(
        dxa_asset_tool_options
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    add_library(
        dxa_asset_importer
        STATIC
            src/ModelImporter.cpp
    )
    target_include_directories(
        dxa_asset_importer
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(
        dxa_asset_importer
        PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
    )
    target_link_libraries(
        dxa_asset_importer
        PUBLIC
            dxa_engine_assets
        PRIVATE
            assimp::assimp
    )
    dxa_enable_project_warnings(dxa_asset_importer)

    add_library(
        dxa_texture_cooker
        STATIC
            src/TextureCooker.cpp
    )
    target_include_directories(
        dxa_texture_cooker
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(
        dxa_texture_cooker
        PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
    )
    target_link_libraries(
        dxa_texture_cooker
        PRIVATE
            Microsoft::DirectXTex
    )
    dxa_enable_project_warnings(dxa_texture_cooker)

    add_executable(dxa_asset_tool src/main.cpp)
    target_compile_definitions(
        dxa_asset_tool
        PRIVATE
            NOMINMAX
            UNICODE
            WIN32_LEAN_AND_MEAN
            _UNICODE
    )
    target_link_libraries(
        dxa_asset_tool
        PRIVATE
            dxa_asset_importer
            dxa_asset_tool_options
            dxa_engine_assets
            dxa_texture_cooker
    )
    dxa_enable_project_warnings(dxa_asset_tool)
endif()
