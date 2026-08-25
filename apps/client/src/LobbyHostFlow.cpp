#include <dxa/client/LobbyHostFlow.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace dxa::client
{
LobbyHostFlow::LobbyHostFlow(const std::uint8_t expectedPlayers)
    : expectedPlayers_{expectedPlayers}
{
    if (expectedPlayers_ < 2U
        || expectedPlayers_ > dxa::protocol::RoomCapacity)
    {
        throw std::invalid_argument{
            "host flow expected players must be between 2 and 24"};
    }
}

HostCommand LobbyHostFlow::OnWelcome(const dxa::protocol::PlayerId player)
{
    if (Terminal())
    {
        return HostCommand::None;
    }
    if (player_.has_value())
    {
        throw std::logic_error{"host flow received duplicate welcome"};
    }
    player_ = player;
    return HostCommand::CreateRoom;
}

HostCommand LobbyHostFlow::OnRoomSnapshot(
    const dxa::protocol::RoomSnapshot& snapshot)
{
    if (Terminal())
    {
        return HostCommand::None;
    }
    if (!player_.has_value())
    {
        throw std::logic_error{"host flow received room before welcome"};
    }
    if (snapshot.host != *player_)
    {
        throw std::logic_error{"network create client is no longer room host"};
    }
    if (snapshot.members.size() > expectedPlayers_)
    {
        throw std::logic_error{"room exceeds expected player count"};
    }
    if (room_.has_value() && snapshot.room != *room_)
    {
        throw std::logic_error{"host flow room identity changed"};
    }
    room_ = snapshot.room;

    const auto self = std::find_if(
        snapshot.members.begin(),
        snapshot.members.end(),
        [this](const dxa::protocol::RoomMemberView& member) {
            return member.player == *player_;
        });
    if (self == snapshot.members.end())
    {
        throw std::logic_error{"room snapshot omits the host"};
    }
    if (snapshot.state != dxa::protocol::RoomState::Waiting)
    {
        return HostCommand::None;
    }
    if (!self->ready)
    {
        if (readyRequested_)
        {
            return HostCommand::None;
        }
        readyRequested_ = true;
        return HostCommand::SetReady;
    }
    const bool everyMemberReady = std::all_of(
        snapshot.members.begin(),
        snapshot.members.end(),
        [](const dxa::protocol::RoomMemberView& member) {
            return member.ready;
        });
    if (snapshot.members.size() == expectedPlayers_
        && everyMemberReady
        && !startRequested_)
    {
        startRequested_ = true;
        return HostCommand::StartMatch;
    }
    return HostCommand::None;
}

void LobbyHostFlow::OnError(const dxa::protocol::LobbyError error) noexcept
{
    if (!error_.has_value())
    {
        error_ = error;
    }
}

void LobbyHostFlow::OnMatchTicket(
    const dxa::protocol::MatchTicket& ticket)
{
    if (Terminal()
        || !player_.has_value()
        || !room_.has_value()
        || !startRequested_)
    {
        throw std::logic_error{"host flow received ticket before match start"};
    }
    if (ticket.match.value == 0U || ticketReceived_)
    {
        throw std::logic_error{"host flow received invalid or duplicate ticket"};
    }
    ticketReceived_ = true;
}

std::optional<dxa::protocol::PlayerId> LobbyHostFlow::Player() const noexcept
{
    return player_;
}

std::optional<dxa::protocol::RoomId> LobbyHostFlow::Room() const noexcept
{
    return room_;
}

std::optional<dxa::protocol::LobbyError> LobbyHostFlow::Error() const noexcept
{
    return error_;
}

bool LobbyHostFlow::Terminal() const noexcept
{
    return error_.has_value();
}

bool LobbyHostFlow::TicketReceived() const noexcept
{
    return ticketReceived_;
}
} // namespace dxa::client
