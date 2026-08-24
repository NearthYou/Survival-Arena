#pragma once

#include <dxa/protocol/LobbyMessages.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace dxa::lobby
{
struct RoomMember
{
    dxa::protocol::PlayerId player;
    bool ready = false;
    std::uint64_t joinOrdinal = 0;
};

class Room
{
public:
    [[nodiscard]] static Room Create(
        dxa::protocol::RoomId room,
        dxa::protocol::PlayerId host,
        std::uint64_t joinOrdinal);

    [[nodiscard]] std::optional<dxa::protocol::LobbyError> Join(
        dxa::protocol::PlayerId player,
        std::uint64_t joinOrdinal);
    [[nodiscard]] std::optional<dxa::protocol::LobbyError> Leave(
        dxa::protocol::PlayerId player);
    [[nodiscard]] std::optional<dxa::protocol::LobbyError> SetReady(
        dxa::protocol::PlayerId player,
        bool ready);
    [[nodiscard]] std::optional<dxa::protocol::LobbyError> ValidateStart(
        dxa::protocol::PlayerId requester) const;

    void BeginStarting();
    void ReturnToWaiting();
    void MarkInMatch();

    [[nodiscard]] bool Contains(dxa::protocol::PlayerId player) const noexcept;
    [[nodiscard]] bool Empty() const noexcept;
    [[nodiscard]] dxa::protocol::PlayerId Host() const;
    [[nodiscard]] std::vector<dxa::protocol::PlayerId> Players() const;
    [[nodiscard]] dxa::protocol::RoomSnapshot Snapshot(
        std::uint32_t requestId) const;

private:
    Room(
        dxa::protocol::RoomId room,
        dxa::protocol::PlayerId host,
        std::uint64_t joinOrdinal);

    dxa::protocol::RoomId id_;
    dxa::protocol::RoomState state_ = dxa::protocol::RoomState::Waiting;
    dxa::protocol::PlayerId host_;
    std::map<dxa::protocol::PlayerId, RoomMember> members_;
};
} // namespace dxa::lobby
