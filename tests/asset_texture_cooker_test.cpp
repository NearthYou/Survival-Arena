#include <dxa/asset_tool/TextureCooker.hpp>

#include <gtest/gtest.h>

#include <Windows.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace
{
class ComApartment
{
public:
    ComApartment() : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}

    ~ComApartment()
    {
        if (SUCCEEDED(result_))
        {
            CoUninitialize();
        }
    }

    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;

    [[nodiscard]] HRESULT Result() const noexcept
    {
        return result_;
    }

private:
    HRESULT result_;
};

ComApartment GlobalComApartment;

class TextureTemporaryDirectory
{
public:
    TextureTemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path()
            / ("dxa-texture-cooker-" + std::to_string(suffix));
        std::filesystem::create_directories(path_);
    }

    ~TextureTemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TextureTemporaryDirectory(const TextureTemporaryDirectory&) = delete;
    TextureTemporaryDirectory& operator=(const TextureTemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void AppendU16(std::vector<std::uint8_t>& bytes, const std::uint16_t value)
{
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void AppendU32(std::vector<std::uint8_t>& bytes, const std::uint32_t value)
{
    for (std::uint32_t shift = 0; shift < 32U; shift += 8U)
    {
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

void WriteBmp4x4(const std::filesystem::path& path)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(102);
    bytes.push_back('B');
    bytes.push_back('M');
    AppendU32(bytes, 102);
    AppendU16(bytes, 0);
    AppendU16(bytes, 0);
    AppendU32(bytes, 54);
    AppendU32(bytes, 40);
    AppendU32(bytes, 4);
    AppendU32(bytes, 4);
    AppendU16(bytes, 1);
    AppendU16(bytes, 24);
    AppendU32(bytes, 0);
    AppendU32(bytes, 48);
    AppendU32(bytes, 2'835);
    AppendU32(bytes, 2'835);
    AppendU32(bytes, 0);
    AppendU32(bytes, 0);
    for (std::uint8_t y = 0; y < 4; ++y)
    {
        for (std::uint8_t x = 0; x < 4; ++x)
        {
            bytes.push_back(static_cast<std::uint8_t>(32U + x * 48U));
            bytes.push_back(static_cast<std::uint8_t>(32U + y * 48U));
            bytes.push_back(static_cast<std::uint8_t>(224U - x * 32U));
        }
    }
    ASSERT_EQ(102U, bytes.size());

    std::ofstream file{path, std::ios::binary | std::ios::trunc};
    ASSERT_TRUE(file) << path;
    file.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    ASSERT_TRUE(file) << path;
}

[[nodiscard]] std::vector<std::uint8_t> ReadBytes(const std::filesystem::path& path)
{
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    EXPECT_TRUE(file) << path;
    const std::streampos end = file.tellg();
    EXPECT_GT(end, 0);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    EXPECT_TRUE(file) << path;
    return bytes;
}

[[nodiscard]] std::uint32_t ReadU32(
    const std::span<const std::uint8_t> bytes,
    const std::size_t offset)
{
    EXPECT_LE(offset + 4, bytes.size());
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 8U
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 16U
        | static_cast<std::uint32_t>(bytes[offset + 3]) << 24U;
}

TEST(TextureCooker, WritesBc7SrgbDdsWithCompleteMipChain)
{
    ASSERT_TRUE(SUCCEEDED(GlobalComApartment.Result()));

    TextureTemporaryDirectory directory;
    const std::filesystem::path source = directory.Path() / "atlas.bmp";
    const std::filesystem::path output = directory.Path() / "atlas.dds";
    WriteBmp4x4(source);

    dxa::asset_tool::CookTexture(source, output);

    const std::vector<std::uint8_t> bytes = ReadBytes(output);
    ASSERT_GE(bytes.size(), 148U);
    EXPECT_EQ((std::array<std::uint8_t, 4>{'D', 'D', 'S', ' '}),
        (std::array<std::uint8_t, 4>{bytes[0], bytes[1], bytes[2], bytes[3]}));
    EXPECT_EQ(4U, ReadU32(bytes, 12));
    EXPECT_EQ(4U, ReadU32(bytes, 16));
    EXPECT_EQ(3U, ReadU32(bytes, 28));
    EXPECT_EQ((std::array<std::uint8_t, 4>{'D', 'X', '1', '0'}),
        (std::array<std::uint8_t, 4>{bytes[84], bytes[85], bytes[86], bytes[87]}));
    EXPECT_EQ(99U, ReadU32(bytes, 128));
}

TEST(TextureCooker, RejectsMissingSourceImage)
{
    ASSERT_TRUE(SUCCEEDED(GlobalComApartment.Result()));

    TextureTemporaryDirectory directory;

    EXPECT_THROW(
        dxa::asset_tool::CookTexture(
            directory.Path() / "missing.png",
            directory.Path() / "missing.dds"),
        dxa::asset_tool::TextureCookError);
}
} // namespace
