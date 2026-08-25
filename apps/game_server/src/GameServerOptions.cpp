#include <dxa/game_server/GameServerOptions.hpp>

#include <boost/asio/ip/address.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace dxa::game_server
{
namespace
{
[[nodiscard]] std::optional<std::uint32_t> ParseU32(
    const std::string_view text) noexcept
{
    std::uint32_t value = 0U;
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

[[nodiscard]] std::optional<std::uint16_t> ParsePort(
    const std::string_view text) noexcept
{
    const auto value = ParseU32(text);
    if (!value.has_value() || *value == 0U || *value > 65535U)
    {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(*value);
}

[[nodiscard]] bool IsVisibleAsciiHost(const std::string_view host) noexcept
{
    if (host.empty() || host.size() > 255U)
    {
        return false;
    }
    return std::all_of(host.begin(), host.end(), [](const char character) {
        const auto value = static_cast<unsigned char>(character);
        return value >= 0x21U && value <= 0x7EU;
    });
}

[[nodiscard]] GameServerOptionsParseResult Failure(std::string error)
{
    return {std::nullopt, std::move(error)};
}
} // namespace

GameServerOptionsParseResult ParseGameServerOptions(
    const std::span<const std::string_view> arguments)
{
    GameServerOptions options;
    std::array<bool, 9U> seen{};
    for (std::size_t index = 0U; index < arguments.size(); ++index)
    {
        const std::string_view option = arguments[index];
        if (index + 1U >= arguments.size())
        {
            return Failure("option requires a value: " + std::string{option});
        }
        const std::string_view value = arguments[++index];

        std::size_t slot = seen.size();
        if (option == "--lobby-control-host")
        {
            slot = 0U;
            options.lobbyControlHost = value;
        }
        else if (option == "--lobby-control-port")
        {
            slot = 1U;
            const auto parsed = ParsePort(value);
            if (!parsed.has_value())
            {
                return Failure(
                    "--lobby-control-port must be between 1 and 65535");
            }
            options.lobbyControlPort = *parsed;
        }
        else if (option == "--worker-id")
        {
            slot = 2U;
            const auto parsed = ParseU32(value);
            if (!parsed.has_value() || *parsed == 0U)
            {
                return Failure("--worker-id must be a nonzero uint32");
            }
            options.worker = dxa::protocol::WorkerId{*parsed};
        }
        else if (option == "--advertise-host")
        {
            slot = 3U;
            options.advertisedHost = value;
        }
        else if (option == "--game-bind")
        {
            slot = 4U;
            options.gameBindAddress = value;
        }
        else if (option == "--game-tcp-port")
        {
            slot = 5U;
            const auto parsed = ParsePort(value);
            if (!parsed.has_value())
            {
                return Failure(
                    "--game-tcp-port must be between 1 and 65535");
            }
            options.gameTcpPort = *parsed;
        }
        else if (option == "--game-udp-port")
        {
            slot = 6U;
            const auto parsed = ParsePort(value);
            if (!parsed.has_value())
            {
                return Failure(
                    "--game-udp-port must be between 1 and 65535");
            }
            options.gameUdpPort = *parsed;
        }
        else if (option == "--replication-mode")
        {
            slot = 7U;
            if (value != "full-state")
            {
                return Failure(
                    "--replication-mode must be full-state");
            }
            options.replicationMode =
                dxa::protocol::ReplicationMode::FullState;
        }
        else if (option == "--metrics-output-root")
        {
            slot = 8U;
            if (value.empty() || value.size() > 4096U)
            {
                return Failure(
                    "--metrics-output-root must contain 1 to 4096 bytes");
            }
            options.metricsOutputRoot = value;
        }
        else
        {
            return Failure("unknown option: " + std::string{option});
        }

        if (seen[slot])
        {
            return Failure("duplicate option: " + std::string{option});
        }
        seen[slot] = true;
    }

    boost::system::error_code addressError;
    static_cast<void>(boost::asio::ip::make_address(
        options.lobbyControlHost,
        addressError));
    if (addressError)
    {
        return Failure(
            "--lobby-control-host must be a numeric IP address");
    }
    addressError.clear();
    static_cast<void>(boost::asio::ip::make_address(
        options.gameBindAddress,
        addressError));
    if (addressError)
    {
        return Failure("--game-bind must be a numeric IP address");
    }
    if (!IsVisibleAsciiHost(options.advertisedHost))
    {
        return Failure(
            "--advertise-host must be visible ASCII and at most 255 bytes");
    }
    return {std::move(options), {}};
}
} // namespace dxa::game_server
