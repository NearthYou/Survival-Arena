#include <dxa/lobby/LobbyServerOptions.hpp>

#include <boost/asio/ip/address.hpp>

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

    return {std::move(options), {}};
}
} // namespace dxa::lobby
