#include <dxa/asset_tool/AssetToolOptions.hpp>
#include <dxa/asset_tool/ModelImporter.hpp>
#include <dxa/asset_tool/TextureCooker.hpp>
#include <dxa/engine/assets/AssetFile.hpp>

#include <Windows.h>
#include <objbase.h>

#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
class ComApartment
{
public:
    ComApartment() : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED))
    {
        if (FAILED(result_))
        {
            throw std::runtime_error{"CoInitializeEx failed"};
        }
    }

    ~ComApartment()
    {
        CoUninitialize();
    }

    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;

private:
    HRESULT result_;
};

void EnsureOutputDirectory(const std::filesystem::path& outputPath)
{
    if (!outputPath.parent_path().empty())
    {
        std::filesystem::create_directories(outputPath.parent_path());
    }
}
} // namespace

int wmain(const int argc, wchar_t* const* argv)
{
    std::vector<std::wstring_view> arguments;
    arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index)
    {
        arguments.emplace_back(argv[index]);
    }

    const dxa::asset_tool::AssetToolOptionsParseResult parsed =
        dxa::asset_tool::ParseAssetToolOptions(arguments);
    if (!parsed.options.has_value())
    {
        std::cerr << parsed.error << '\n';
        std::cerr << "usage: dxa_asset_tool <model|texture> --input <path> --output <path> "
                     "[--sample-rate <hz>]\n";
        return 1;
    }

    try
    {
        ComApartment apartment;
        const dxa::asset_tool::AssetToolOptions& options = *parsed.options;
        EnsureOutputDirectory(options.outputPath);
        if (options.command == dxa::asset_tool::AssetCommand::Model)
        {
            const dxa::engine::asset::ModelAsset model = dxa::asset_tool::ImportModel(
                options.inputPath,
                dxa::asset_tool::ModelImportOptions{options.animationSampleRate});
            dxa::engine::asset::SaveModelAsset(options.outputPath, model);
            std::wcout << L"model cooked: " << options.outputPath.native()
                       << L" (vertices=" << model.vertices.size()
                       << L", indices=" << model.indices.size()
                       << L", joints=" << model.joints.size()
                       << L", animations=" << model.animations.size() << L")\n";
        }
        else
        {
            dxa::asset_tool::CookTexture(options.inputPath, options.outputPath);
            std::wcout << L"texture cooked: " << options.outputPath.native() << L'\n';
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "asset cook failed: " << error.what() << '\n';
        return 2;
    }
}
