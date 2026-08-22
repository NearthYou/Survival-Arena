#pragma once

#include <cstdint>
#include <charconv>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace dxa::client
{
enum class AdapterType
{
    Hardware,
    Warp
};

struct ClientOptions
{
    AdapterType adapter = AdapterType::Hardware;
    bool hidden = false;
    bool vsync = true;
    std::uint32_t frameLimit = 0;
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
};

struct ClientOptionsParseResult
{
    std::optional<ClientOptions> options;
    std::string error;
};

namespace detail
{
[[nodiscard]] inline std::optional<std::uint32_t> ParseUnsigned(
    const std::string_view value) noexcept
{
    std::uint32_t parsed = 0;
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end)
    {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] inline ClientOptionsParseResult Error(std::string message)
{
    return ClientOptionsParseResult{std::nullopt, std::move(message)};
}
} // namespace detail

[[nodiscard]] inline ClientOptionsParseResult ParseClientOptions(
    const std::span<const std::string_view> arguments)
{
    ClientOptions options;

    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const std::string_view argument = arguments[index];
        if (argument == "--warp")
        {
            options.adapter = AdapterType::Warp;
        }
        else if (argument == "--hidden")
        {
            options.hidden = true;
        }
        else if (argument == "--no-vsync")
        {
            options.vsync = false;
        }
        else if (argument == "--frames" || argument == "--width" || argument == "--height")
        {
            if (index + 1 >= arguments.size())
            {
                return detail::Error(std::string{argument} + " requires a value");
            }

            const auto parsed = detail::ParseUnsigned(arguments[++index]);
            if (!parsed.has_value())
            {
                return detail::Error(std::string{argument} + " must be an unsigned integer");
            }

            if (argument == "--frames")
            {
                options.frameLimit = *parsed;
            }
            else
            {
                constexpr std::uint32_t MaximumDimension = 16384;
                if (*parsed == 0 || *parsed > MaximumDimension)
                {
                    return detail::Error(
                        std::string{argument} + " must be between 1 and 16384");
                }

                if (argument == "--width")
                {
                    options.width = *parsed;
                }
                else
                {
                    options.height = *parsed;
                }
            }
        }
        else
        {
            return detail::Error("unknown argument: " + std::string{argument});
        }
    }

    return ClientOptionsParseResult{options, {}};
}
} // namespace dxa::client
