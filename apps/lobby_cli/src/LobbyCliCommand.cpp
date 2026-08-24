#include <dxa/lobby_cli/LobbyCliCommand.hpp>

#include <charconv>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace dxa::lobby_cli
{
namespace
{
[[nodiscard]] LobbyCliCommandParseResult Failure(std::string error)
{
    return {std::nullopt, std::move(error)};
}

[[nodiscard]] std::optional<dxa::protocol::RoomId> ParseRoom(
    const std::string& text) noexcept
{
    std::uint64_t value = 0U;
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value);
    if (error != std::errc{}
        || end != text.data() + text.size()
        || value == 0U
        || value > std::numeric_limits<std::uint32_t>::max())
    {
        return std::nullopt;
    }
    return dxa::protocol::RoomId{static_cast<std::uint32_t>(value)};
}
} // namespace

LobbyCliCommandParseResult ParseLobbyCliCommand(const std::string_view line)
{
    std::istringstream input{std::string{line}};
    std::vector<std::string> tokens;
    for (std::string token; input >> token;)
    {
        tokens.push_back(std::move(token));
    }
    if (tokens.empty())
    {
        return Failure("command is empty");
    }

    const std::string& name = tokens.front();
    if (name == "list" || name == "create" || name == "leave"
        || name == "start" || name == "quit")
    {
        if (tokens.size() != 1U)
        {
            return Failure("command does not accept arguments");
        }
        LobbyCliCommandType type = LobbyCliCommandType::List;
        if (name == "create")
        {
            type = LobbyCliCommandType::Create;
        }
        else if (name == "leave")
        {
            type = LobbyCliCommandType::Leave;
        }
        else if (name == "start")
        {
            type = LobbyCliCommandType::Start;
        }
        else if (name == "quit")
        {
            type = LobbyCliCommandType::Quit;
        }
        return {LobbyCliCommand{type, {}, false}, {}};
    }

    if (name == "join")
    {
        if (tokens.size() != 2U)
        {
            return Failure("join requires one room ID");
        }
        const auto room = ParseRoom(tokens[1]);
        if (!room.has_value())
        {
            return Failure("room ID must be between 1 and 4294967295");
        }
        return {
            LobbyCliCommand{LobbyCliCommandType::Join, *room, false},
            {}};
    }

    if (name == "ready")
    {
        if (tokens.size() != 2U
            || (tokens[1] != "on" && tokens[1] != "off"))
        {
            return Failure("ready requires on or off");
        }
        return {
            LobbyCliCommand{
                LobbyCliCommandType::Ready,
                {},
                tokens[1] == "on"},
            {}};
    }
    return Failure("unknown command");
}
} // namespace dxa::lobby_cli
