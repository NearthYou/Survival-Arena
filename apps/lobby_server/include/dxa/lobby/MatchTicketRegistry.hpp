#pragma once

#include <dxa/protocol/Ids.hpp>
#include <dxa/protocol/LobbyTypes.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <map>
#include <optional>
#include <span>

namespace dxa::lobby
{
using MatchTicketValue = std::array<std::byte, dxa::protocol::MatchTicketBytes>;

enum class TicketConsumeResult
{
    Accepted,
    NotFound,
    Expired,
    Mismatch
};

class ITicketSource
{
public:
    virtual ~ITicketSource() = default;

    [[nodiscard]] virtual bool Fill(
        std::span<std::byte, dxa::protocol::MatchTicketBytes> output) noexcept = 0;
};

class SecureTicketSource final : public ITicketSource
{
public:
    [[nodiscard]] bool Fill(
        std::span<std::byte, dxa::protocol::MatchTicketBytes> output) noexcept override;
};

class MatchTicketRegistry
{
public:
    explicit MatchTicketRegistry(ITicketSource& source) noexcept;

    [[nodiscard]] std::optional<MatchTicketValue> Issue(
        dxa::protocol::MatchId match,
        dxa::protocol::PlayerId player,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] TicketConsumeResult Consume(
        const MatchTicketValue& ticket,
        dxa::protocol::MatchId match,
        dxa::protocol::PlayerId player,
        std::chrono::steady_clock::time_point now);
    void Revoke(std::span<const MatchTicketValue> tickets) noexcept;
    void PurgeExpired(std::chrono::steady_clock::time_point now) noexcept;

private:
    struct TicketRecord
    {
        dxa::protocol::MatchId match;
        dxa::protocol::PlayerId player;
        std::chrono::steady_clock::time_point expiresAt;
    };

    ITicketSource& source_;
    std::map<MatchTicketValue, TicketRecord> tickets_;
};
} // namespace dxa::lobby
