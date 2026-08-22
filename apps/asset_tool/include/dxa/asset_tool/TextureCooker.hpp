#pragma once

#include <filesystem>
#include <stdexcept>

namespace dxa::asset_tool
{
class TextureCookError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

void CookTexture(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& outputPath);
} // namespace dxa::asset_tool
