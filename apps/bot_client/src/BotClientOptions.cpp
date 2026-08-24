#include <dxa/bot_client/BotClientOptions.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <system_error>
#include <utility>

namespace dxa::bot_client
{
namespace
{
[[nodiscard]] std::optional<std::uint64_t> ParseUnsigned(
    const std::string_view text) noexcept
{
    std::uint64_t value = 0U;
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value);
    if (error != std::errc{} || end != text.data() + text.size())
    {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] BotClientOptionsParseResult Failure(std::string error)
{
    return {std::nullopt, std::move(error)};
}
} // namespace

BotClientOptionsParseResult ParseBotClientOptions(
    const std::span<const std::string_view> arguments)
{
    BotClientOptions options;
    bool sawHost = false;
    bool sawPort = false;
    bool sawRoom = false;
    bool sawCount = false;
    bool sawPlay = false;
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const std::string_view option = arguments[index];
        if (option == "--play")
        {
            if (sawPlay)
            {
                return Failure("unknown or duplicate option: --play");
            }
            sawPlay = true;
            options.play = true;
            continue;
        }
        if (index + 1U >= arguments.size())
        {
            return Failure("option requires a value: " + std::string{option});
        }
        const std::string_view value = arguments[++index];
        if (option == "--host" && !sawHost)
        {
            sawHost = true;
            options.host = value;
        }
        else if (option == "--port" && !sawPort)
        {
            sawPort = true;
            const auto parsed = ParseUnsigned(value);
            if (!parsed.has_value() || *parsed == 0U || *parsed > 65535U)
            {
                return Failure("port must be between 1 and 65535");
            }
            options.port = static_cast<std::uint16_t>(*parsed);
        }
        else if (option == "--room" && !sawRoom)
        {
            sawRoom = true;
            const auto parsed = ParseUnsigned(value);
            if (!parsed.has_value()
                || *parsed == 0U
                || *parsed > std::numeric_limits<std::uint32_t>::max())
            {
                return Failure("room must be between 1 and 4294967295");
            }
            options.room = {
                static_cast<std::uint32_t>(*parsed)};
        }
        else if (option == "--count" && !sawCount)
        {
            sawCount = true;
            const auto parsed = ParseUnsigned(value);
            if (!parsed.has_value() || *parsed == 0U || *parsed > 23U)
            {
                return Failure("count must be between 1 and 23");
            }
            options.count = static_cast<std::uint32_t>(*parsed);
        }
        else
        {
            return Failure("unknown or duplicate option: " + std::string{option});
        }
    }
    if (!sawRoom)
    {
        return Failure("--room is required");
    }
    if (options.host.empty() || options.host.size() > 255U)
    {
        return Failure("host must contain 1 to 255 bytes");
    }
    if (options.play && options.count != 1U)
    {
        return Failure("--play requires --count 1");
    }
    return {std::move(options), {}};
}
} // namespace dxa::bot_client
