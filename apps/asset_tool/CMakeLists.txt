if(WIN32)
    find_package(assimp CONFIG REQUIRED)

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
endif()
