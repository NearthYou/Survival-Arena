#include <dxa/game_server/GameTicketStore.hpp>

#include <dxa/protocol/LobbyTypes.hpp>

#include <algorithm>
#include <chrono>
#include <set>
#include <stdexcept>

namespace dxa::game_server
{
void GameTicketStore::Load(
    const dxa::protocol::MatchId match,
    const std::span<const dxa::protocol::ReservedParticipant> participants,
    const std::chrono::steady_clock::time_point now,
    const std::chrono::milliseconds lifetime)
{
    if (loaded_)
    {
        throw std::logic_error{"game ticket store can load only once"};
    }
    if (participants.size() < 2U
        || participants.size() > dxa::protocol::RoomCapacity
        || lifetime <= std::chrono::milliseconds::zero())
    {
        throw std::invalid_argument{
            "game tickets require 2 to 24 participants and positive lifetime"};
    }

    std::set<dxa::protocol::PlayerId> players;
    std::set<dxa::protocol::MatchTicketValue> tickets;
    for (const dxa::protocol::ReservedParticipant& participant : participants)
    {
        if (!players.insert(participant.player).second
            || !tickets.insert(participant.ticket).second)
        {
            throw std::invalid_argument{
                "game ticket participants must have unique players and tickets"};
        }
    }

    const auto expiresAt = now + lifetime;
    for (const dxa::protocol::ReservedParticipant& participant : participants)
    {
        tickets_.emplace(
            participant.ticket,
            TicketRecord{match, participant.player, expiresAt});
    }
    loaded_ = true;
}

GameTicketConsumeResult GameTicketStore::Consume(
    const dxa::protocol::MatchTicketValue& ticket,
    const dxa::protocol::MatchId match,
    const dxa::protocol::PlayerId player,
    const std::chrono::steady_clock::time_point now)
{
    if (usedTickets_.contains(ticket))
    {
        return GameTicketConsumeResult::Used;
    }
    const auto found = tickets_.find(ticket);
    if (found == tickets_.end())
    {
        return GameTicketConsumeResult::NotFound;
    }
    if (now >= found->second.expiresAt)
    {
        tickets_.erase(found);
        return GameTicketConsumeResult::Expired;
    }
    if (found->second.match != match || found->second.player != player)
    {
        return GameTicketConsumeResult::Mismatch;
    }

    usedTickets_.insert(ticket);
    tickets_.erase(found);
    return GameTicketConsumeResult::Accepted;
}

std::vector<dxa::protocol::PlayerId> GameTicketStore::PurgeExpired(
    const std::chrono::steady_clock::time_point now)
{
    std::vector<dxa::protocol::PlayerId> expired;
    for (auto ticket = tickets_.begin(); ticket != tickets_.end();)
    {
        if (now < ticket->second.expiresAt)
        {
            ++ticket;
            continue;
        }
        expired.push_back(ticket->second.player);
        ticket = tickets_.erase(ticket);
    }
    std::sort(expired.begin(), expired.end());
    return expired;
}
} // namespace dxa::game_server
