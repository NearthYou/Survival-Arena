#pragma once

#include <charconv>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace dxa::asset_tool
{
enum class AssetCommand
{
    Model,
    Texture
};

struct AssetToolOptions
{
    AssetCommand command = AssetCommand::Model;
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
    float animationSampleRate = 30.0F;
};

struct AssetToolOptionsParseResult
{
    std::optional<AssetToolOptions> options;
    std::string error;
};

namespace detail
{
[[nodiscard]] inline AssetToolOptionsParseResult OptionError(std::string message)
{
    return AssetToolOptionsParseResult{std::nullopt, std::move(message)};
}

[[nodiscard]] inline std::optional<float> ParseSampleRate(const std::wstring_view value)
{
    std::string ascii;
    ascii.reserve(value.size());
    for (const wchar_t character : value)
    {
        if (character < L'0' || character > L'9')
        {
            if (character != L'.')
            {
                return std::nullopt;
            }
        }
        ascii.push_back(static_cast<char>(character));
    }

    float parsed = 0.0F;
    const char* const begin = ascii.data();
    const char* const end = begin + ascii.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed <= 0.0F || parsed > 240.0F)
    {
        return std::nullopt;
    }
    return parsed;
}
} // namespace detail

[[nodiscard]] inline AssetToolOptionsParseResult ParseAssetToolOptions(
    const std::span<const std::wstring_view> arguments)
{
    if (arguments.empty())
    {
        return detail::OptionError("command must be model or texture");
    }

    AssetToolOptions options;
    if (arguments.front() == L"model")
    {
        options.command = AssetCommand::Model;
    }
    else if (arguments.front() == L"texture")
    {
        options.command = AssetCommand::Texture;
    }
    else
    {
        return detail::OptionError("command must be model or texture");
    }

    bool hasInput = false;
    bool hasOutput = false;
    bool hasSampleRate = false;
    for (std::size_t index = 1; index < arguments.size(); ++index)
    {
        const std::wstring_view argument = arguments[index];
        if (argument != L"--input" && argument != L"--output"
            && argument != L"--sample-rate")
        {
            return detail::OptionError("unknown asset tool argument");
        }
        if (index + 1 >= arguments.size())
        {
            return detail::OptionError("asset tool option requires a value");
        }

        const std::wstring_view value = arguments[++index];
        if (argument == L"--input")
        {
            if (hasInput)
            {
                return detail::OptionError("--input may only be specified once");
            }
            options.inputPath = std::filesystem::path{value};
            hasInput = true;
        }
        else if (argument == L"--output")
        {
            if (hasOutput)
            {
                return detail::OptionError("--output may only be specified once");
            }
            options.outputPath = std::filesystem::path{value};
            hasOutput = true;
        }
        else
        {
            if (hasSampleRate)
            {
                return detail::OptionError("--sample-rate may only be specified once");
            }
            const std::optional<float> sampleRate = detail::ParseSampleRate(value);
            if (!sampleRate.has_value())
            {
                return detail::OptionError("--sample-rate must be between 0 and 240");
            }
            options.animationSampleRate = *sampleRate;
            hasSampleRate = true;
        }
    }

    if (!hasInput)
    {
        return detail::OptionError("--input is required");
    }
    if (!hasOutput)
    {
        return detail::OptionError("--output is required");
    }
    if (options.command == AssetCommand::Texture && hasSampleRate)
    {
        return detail::OptionError("--sample-rate is only valid for model cooking");
    }
    return AssetToolOptionsParseResult{std::move(options), {}};
}
} // namespace dxa::asset_tool
