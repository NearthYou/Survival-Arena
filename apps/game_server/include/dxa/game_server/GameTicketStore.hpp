#pragma once

#include <dxa/protocol/WorkerControlMessages.hpp>

#include <chrono>
#include <map>
#include <set>
#include <span>
#include <vector>

namespace dxa::game_server
{
enum class GameTicketConsumeResult
{
    Accepted,
    NotFound,
    Expired,
    Mismatch,
    Used
};

class GameTicketStore
{
public:
    void Load(
        dxa::protocol::MatchId match,
        std::span<const dxa::protocol::ReservedParticipant> participants,
        std::chrono::steady_clock::time_point now,
        std::chrono::milliseconds lifetime);
    [[nodiscard]] GameTicketConsumeResult Consume(
        const dxa::protocol::MatchTicketValue& ticket,
        dxa::protocol::MatchId match,
        dxa::protocol::PlayerId player,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::vector<dxa::protocol::PlayerId> PurgeExpired(
        std::chrono::steady_clock::time_point now);

private:
    struct TicketRecord
    {
        dxa::protocol::MatchId match;
        dxa::protocol::PlayerId player;
        std::chrono::steady_clock::time_point expiresAt;
    };

    std::map<dxa::protocol::MatchTicketValue, TicketRecord> tickets_;
    std::set<dxa::protocol::MatchTicketValue> usedTickets_;
    bool loaded_ = false;
};
} // namespace dxa::game_server
