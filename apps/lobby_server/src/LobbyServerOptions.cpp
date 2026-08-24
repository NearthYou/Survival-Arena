#include <dxa/lobby/LobbyServerOptions.hpp>

#include <boost/asio/ip/address.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <system_error>
#include <utility>

namespace dxa::lobby
{
namespace
{
[[nodiscard]] std::optional<std::uint16_t> ParsePort(
    const std::string_view text) noexcept
{
    std::uint32_t value = 0U;
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value);
    if (error != std::errc{}
        || end != text.data() + text.size()
        || value == 0U
        || value > 65535U)
    {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(value);
}

[[nodiscard]] bool IsValidWorkerHost(const std::string& host) noexcept
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

[[nodiscard]] LobbyServerOptionsParseResult Failure(std::string error)
{
    return {std::nullopt, std::move(error)};
}
} // namespace

LobbyServerOptionsParseResult ParseLobbyServerOptions(
    const std::span<const std::string_view> arguments)
{
    LobbyServerOptions options;
    bool sawBind = false;
    bool sawPort = false;
    bool sawWorkerHost = false;
    bool sawWorkerTcp = false;
    bool sawWorkerUdp = false;
    std::string workerHost;
    std::uint16_t workerTcp = 0U;
    std::uint16_t workerUdp = 0U;

    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const std::string_view option = arguments[index];
        if (index + 1U >= arguments.size())
        {
            return Failure("option requires a value: " + std::string{option});
        }
        const std::string_view value = arguments[++index];

        if (option == "--bind")
        {
            if (sawBind)
            {
                return Failure("duplicate --bind option");
            }
            sawBind = true;
            options.bindAddress = value;
        }
        else if (option == "--port")
        {
            if (sawPort)
            {
                return Failure("duplicate --port option");
            }
            sawPort = true;
            const auto parsed = ParsePort(value);
            if (!parsed.has_value())
            {
                return Failure("--port must be between 1 and 65535");
            }
            options.port = *parsed;
        }
        else if (option == "--worker-host")
        {
            if (sawWorkerHost)
            {
                return Failure("duplicate --worker-host option");
            }
            sawWorkerHost = true;
            workerHost = value;
        }
        else if (option == "--worker-tcp-port")
        {
            if (sawWorkerTcp)
            {
                return Failure("duplicate --worker-tcp-port option");
            }
            sawWorkerTcp = true;
            const auto parsed = ParsePort(value);
            if (!parsed.has_value())
            {
                return Failure("--worker-tcp-port must be between 1 and 65535");
            }
            workerTcp = *parsed;
        }
        else if (option == "--worker-udp-port")
        {
            if (sawWorkerUdp)
            {
                return Failure("duplicate --worker-udp-port option");
            }
            sawWorkerUdp = true;
            const auto parsed = ParsePort(value);
            if (!parsed.has_value())
            {
                return Failure("--worker-udp-port must be between 1 and 65535");
            }
            workerUdp = *parsed;
        }
        else
        {
            return Failure("unknown option: " + std::string{option});
        }
    }

    boost::system::error_code addressError;
    static_cast<void>(boost::asio::ip::make_address(
        options.bindAddress,
        addressError));
    if (addressError)
    {
        return Failure("--bind must be a numeric IP address");
    }

    const std::size_t workerFields = static_cast<std::size_t>(sawWorkerHost)
        + static_cast<std::size_t>(sawWorkerTcp)
        + static_cast<std::size_t>(sawWorkerUdp);
    if (workerFields != 0U && workerFields != 3U)
    {
        return Failure("worker host, TCP port and UDP port must be provided together");
    }
    if (workerFields == 3U)
    {
        if (!IsValidWorkerHost(workerHost))
        {
            return Failure("worker host must be visible ASCII and at most 255 bytes");
        }
        options.worker = GameEndpoint{
            std::move(workerHost),
            workerTcp,
            workerUdp};
    }
    return {std::move(options), {}};
}
} // namespace dxa::lobby
