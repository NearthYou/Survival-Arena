#include <dxa/asset_tool/TextureCooker.hpp>

#include <DirectXTex.h>

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

namespace dxa::asset_tool
{
namespace
{
void RequireSuccess(const HRESULT result, const char* operation)
{
    if (FAILED(result))
    {
        std::ostringstream message;
        message << operation << " failed with HRESULT 0x" << std::hex << std::uppercase
                << static_cast<std::uint32_t>(result);
        throw TextureCookError{message.str()};
    }
}
} // namespace

void CookTexture(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& outputPath)
{
    DirectX::TexMetadata sourceMetadata{};
    DirectX::ScratchImage source;
    RequireSuccess(
        DirectX::LoadFromWICFile(
            sourcePath.c_str(),
            DirectX::WIC_FLAGS_FORCE_RGB | DirectX::WIC_FLAGS_FORCE_SRGB,
            &sourceMetadata,
            source),
        "DirectX::LoadFromWICFile");
    if (sourceMetadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D
        || sourceMetadata.arraySize != 1 || sourceMetadata.depth != 1)
    {
        throw TextureCookError{"only a single 2D color texture can be cooked"};
    }

    DirectX::ScratchImage mipChain;
    RequireSuccess(
        DirectX::GenerateMipMaps(
            source.GetImages(),
            source.GetImageCount(),
            source.GetMetadata(),
            DirectX::TEX_FILTER_DEFAULT,
            0,
            mipChain),
        "DirectX::GenerateMipMaps");

    DirectX::ScratchImage compressed;
    RequireSuccess(
        DirectX::Compress(
            mipChain.GetImages(),
            mipChain.GetImageCount(),
            mipChain.GetMetadata(),
            DXGI_FORMAT_BC7_UNORM_SRGB,
            DirectX::TEX_COMPRESS_DEFAULT,
            DirectX::TEX_THRESHOLD_DEFAULT,
            compressed),
        "DirectX::Compress(BC7_UNORM_SRGB)");

    if (!outputPath.parent_path().empty())
    {
        std::filesystem::create_directories(outputPath.parent_path());
    }
    RequireSuccess(
        DirectX::SaveToDDSFile(
            compressed.GetImages(),
            compressed.GetImageCount(),
            compressed.GetMetadata(),
            DirectX::DDS_FLAGS_NONE,
            outputPath.c_str()),
        "DirectX::SaveToDDSFile");
}
} // namespace dxa::asset_tool
