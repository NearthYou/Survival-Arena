#pragma once

#include <dxa/protocol/LobbyMessages.hpp>

#include <cstdint>
#include <optional>

namespace dxa::client
{
enum class HostCommand
{
    None,
    CreateRoom,
    SetReady,
    StartMatch
};

class LobbyHostFlow
{
public:
    explicit LobbyHostFlow(std::uint8_t expectedPlayers);

    [[nodiscard]] HostCommand OnWelcome(dxa::protocol::PlayerId player);
    [[nodiscard]] HostCommand OnRoomSnapshot(
        const dxa::protocol::RoomSnapshot& snapshot);
    void OnError(dxa::protocol::LobbyError error) noexcept;
    void OnMatchTicket(const dxa::protocol::MatchTicket& ticket);

    [[nodiscard]] std::optional<dxa::protocol::PlayerId> Player() const noexcept;
    [[nodiscard]] std::optional<dxa::protocol::RoomId> Room() const noexcept;
    [[nodiscard]] std::optional<dxa::protocol::LobbyError> Error() const noexcept;
    [[nodiscard]] bool Terminal() const noexcept;
    [[nodiscard]] bool TicketReceived() const noexcept;

private:
    std::uint8_t expectedPlayers_ = 0U;
    std::optional<dxa::protocol::PlayerId> player_;
    std::optional<dxa::protocol::RoomId> room_;
    std::optional<dxa::protocol::LobbyError> error_;
    bool readyRequested_ = false;
    bool startRequested_ = false;
    bool ticketReceived_ = false;
};
} // namespace dxa::client
