#include <dxa/game_server/GameServerOptions.hpp>

#include <dxa/protocol/DatagramShaper.hpp>

#include <boost/asio/ip/address.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <stdexcept>
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

[[nodiscard]] bool IsAsciiAlphanumeric(const char value) noexcept
{
    return (value >= '0' && value <= '9')
        || (value >= 'A' && value <= 'Z')
        || (value >= 'a' && value <= 'z');
}

[[nodiscard]] bool IsDnsHost(const std::string_view host) noexcept
{
    if (host.empty() || host.size() > 253U)
    {
        return false;
    }

    std::size_t labelStart = 0U;
    while (labelStart < host.size())
    {
        const std::size_t dot = host.find('.', labelStart);
        const std::size_t labelEnd =
            dot == std::string_view::npos ? host.size() : dot;
        const std::size_t labelLength = labelEnd - labelStart;
        if (labelLength == 0U
            || labelLength > 63U
            || !IsAsciiAlphanumeric(host[labelStart])
            || !IsAsciiAlphanumeric(host[labelEnd - 1U]))
        {
            return false;
        }
        for (std::size_t index = labelStart; index < labelEnd; ++index)
        {
            if (!IsAsciiAlphanumeric(host[index]) && host[index] != '-')
            {
                return false;
            }
        }
        if (dot == std::string_view::npos)
        {
            return true;
        }
        labelStart = dot + 1U;
    }
    return false;
}

[[nodiscard]] bool IsIpAddressOrDnsHost(
    const std::string_view host) noexcept
{
    boost::system::error_code addressError;
    static_cast<void>(boost::asio::ip::make_address(host, addressError));
    return !addressError || IsDnsHost(host);
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
    std::array<bool, 13U> seen{};
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
            if (value == "full-state")
            {
                options.replicationMode =
                    dxa::protocol::ReplicationMode::FullState;
            }
            else if (value == "interest-full")
            {
                options.replicationMode =
                    dxa::protocol::ReplicationMode::InterestFullPrecision;
            }
            else if (value == "interest-quantized")
            {
                options.replicationMode =
                    dxa::protocol::ReplicationMode::InterestQuantized;
            }
            else if (value == "interest-delta")
            {
                options.replicationMode =
                    dxa::protocol::ReplicationMode::InterestDelta;
            }
            else
            {
                return Failure(
                    "--replication-mode must be full-state, interest-full, "
                    "interest-quantized or interest-delta");
            }
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
        else if (option == "--udp-latency-ms")
        {
            slot = 9U;
            const auto parsed = ParseU32(value);
            if (!parsed.has_value())
            {
                return Failure("--udp-latency-ms must be a uint32");
            }
            options.udpImpairment.oneWayLatency =
                std::chrono::milliseconds{*parsed};
        }
        else if (option == "--udp-jitter-ms")
        {
            slot = 10U;
            const auto parsed = ParseU32(value);
            if (!parsed.has_value())
            {
                return Failure("--udp-jitter-ms must be a uint32");
            }
            options.udpImpairment.jitter =
                std::chrono::milliseconds{*parsed};
        }
        else if (option == "--udp-loss-basis-points")
        {
            slot = 11U;
            const auto parsed = ParseU32(value);
            if (!parsed.has_value())
            {
                return Failure(
                    "--udp-loss-basis-points must be a uint32");
            }
            options.udpImpairment.lossBasisPoints = *parsed;
        }
        else if (option == "--network-seed")
        {
            slot = 12U;
            const auto parsed = ParseU32(value);
            if (!parsed.has_value())
            {
                return Failure("--network-seed must be a uint32");
            }
            options.udpImpairment.seed = *parsed;
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

    if (!IsIpAddressOrDnsHost(options.lobbyControlHost))
    {
        return Failure(
            "--lobby-control-host must be an IP address or DNS hostname");
    }
    boost::system::error_code addressError;
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
    try
    {
        static_cast<void>(dxa::protocol::DatagramShaper{
            options.udpImpairment,
            dxa::protocol::DatagramDirection::ServerToClient});
    }
    catch (const std::invalid_argument& error)
    {
        return Failure(error.what());
    }
    return {std::move(options), {}};
}
} // namespace dxa::game_server
