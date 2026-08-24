#include <dxa/lobby/Room.hpp>

#include <algorithm>
#include <stdexcept>

namespace dxa::lobby
{
using dxa::protocol::LobbyError;
using dxa::protocol::PlayerId;
using dxa::protocol::RoomCapacity;
using dxa::protocol::RoomId;
using dxa::protocol::RoomMemberView;
using dxa::protocol::RoomSnapshot;
using dxa::protocol::RoomState;

Room Room::Create(
    const RoomId room,
    const PlayerId host,
    const std::uint64_t joinOrdinal)
{
    return Room{room, host, joinOrdinal};
}

Room::Room(
    const RoomId room,
    const PlayerId host,
    const std::uint64_t joinOrdinal)
    : id_{room},
      host_{host}
{
    members_.emplace(host, RoomMember{host, false, joinOrdinal});
}

std::optional<LobbyError> Room::Join(
    const PlayerId player,
    const std::uint64_t joinOrdinal)
{
    if (state_ != RoomState::Waiting)
    {
        return LobbyError::RoomNotJoinable;
    }
    if (Contains(player))
    {
        return LobbyError::AlreadyInRoom;
    }
    if (members_.size() >= RoomCapacity)
    {
        return LobbyError::RoomFull;
    }
    members_.emplace(player, RoomMember{player, false, joinOrdinal});
    return std::nullopt;
}

std::optional<LobbyError> Room::Leave(const PlayerId player)
{
    if (state_ != RoomState::Waiting)
    {
        return LobbyError::RoomNotJoinable;
    }
    const auto member = members_.find(player);
    if (member == members_.end())
    {
        return LobbyError::NotInRoom;
    }

    const bool leavingHost = player == host_;
    members_.erase(member);
    if (leavingHost && !members_.empty())
    {
        const auto successor = std::min_element(
            members_.begin(),
            members_.end(),
            [](const auto& left, const auto& right) {
                if (left.second.joinOrdinal != right.second.joinOrdinal)
                {
                    return left.second.joinOrdinal < right.second.joinOrdinal;
                }
                return left.first < right.first;
            });
        host_ = successor->first;
    }
    return std::nullopt;
}

std::optional<LobbyError> Room::SetReady(
    const PlayerId player,
    const bool ready)
{
    if (state_ != RoomState::Waiting)
    {
        return LobbyError::RoomNotJoinable;
    }
    const auto member = members_.find(player);
    if (member == members_.end())
    {
        return LobbyError::NotInRoom;
    }
    member->second.ready = ready;
    return std::nullopt;
}

std::optional<LobbyError> Room::ValidateStart(const PlayerId requester) const
{
    if (state_ != RoomState::Waiting)
    {
        return LobbyError::RoomNotJoinable;
    }
    if (members_.empty() || requester != host_)
    {
        return LobbyError::NotHost;
    }
    if (members_.size() < 2U)
    {
        return LobbyError::MinimumPlayersRequired;
    }
    const bool allReady = std::all_of(
        members_.begin(),
        members_.end(),
        [](const auto& member) { return member.second.ready; });
    return allReady
        ? std::nullopt
        : std::optional<LobbyError>{LobbyError::NotAllReady};
}

void Room::BeginStarting()
{
    if (ValidateStart(host_).has_value())
    {
        throw std::logic_error{"room cannot begin starting before start validation"};
    }
    state_ = RoomState::Starting;
}

void Room::ReturnToWaiting()
{
    if (state_ != RoomState::Starting)
    {
        throw std::logic_error{"room can return only from Starting"};
    }
    state_ = RoomState::Waiting;
}

void Room::MarkInMatch()
{
    if (state_ != RoomState::Starting)
    {
        throw std::logic_error{"room can enter match only from Starting"};
    }
    state_ = RoomState::InMatch;
}

bool Room::Contains(const PlayerId player) const noexcept
{
    return members_.contains(player);
}

bool Room::Empty() const noexcept
{
    return members_.empty();
}

PlayerId Room::Host() const
{
    if (members_.empty())
    {
        throw std::logic_error{"empty room has no host"};
    }
    return host_;
}

std::vector<PlayerId> Room::Players() const
{
    std::vector<PlayerId> players;
    players.reserve(members_.size());
    for (const auto& [player, member] : members_)
    {
        static_cast<void>(member);
        players.push_back(player);
    }
    return players;
}

RoomSnapshot Room::Snapshot(const std::uint32_t requestId) const
{
    if (members_.empty())
    {
        throw std::logic_error{"empty room has no snapshot"};
    }
    std::vector<RoomMemberView> members;
    members.reserve(members_.size());
    for (const auto& [player, member] : members_)
    {
        members.push_back(RoomMemberView{player, member.ready});
    }
    return RoomSnapshot{
        requestId,
        id_,
        state_,
        host_,
        std::move(members)};
}
} // namespace dxa::lobby
