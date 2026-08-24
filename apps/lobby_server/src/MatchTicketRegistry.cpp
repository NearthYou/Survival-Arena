#include <dxa/lobby/MatchTicketRegistry.hpp>

#include <cstdint>

namespace dxa::lobby
{
namespace
{
constexpr std::uint32_t MaximumTicketAttempts = 8U;
} // namespace

MatchTicketRegistry::MatchTicketRegistry(ITicketSource& source) noexcept
    : source_{source}
{
}

std::optional<MatchTicketValue> MatchTicketRegistry::Issue(
    const dxa::protocol::MatchId match,
    const dxa::protocol::PlayerId player,
    const std::chrono::steady_clock::time_point now)
{
    PurgeExpired(now);
    for (std::uint32_t attempt = 0; attempt < MaximumTicketAttempts; ++attempt)
    {
        static_cast<void>(attempt);
        MatchTicketValue ticket{};
        if (!source_.Fill(ticket))
        {
            return std::nullopt;
        }
        const auto [iterator, inserted] = tickets_.emplace(
            ticket,
            TicketRecord{
                match,
                player,
                now + std::chrono::seconds{
                    static_cast<std::chrono::seconds::rep>(
                        dxa::protocol::MatchTicketLifetimeSeconds)}});
        static_cast<void>(iterator);
        if (inserted)
        {
            return ticket;
        }
    }
    return std::nullopt;
}

TicketConsumeResult MatchTicketRegistry::Consume(
    const MatchTicketValue& ticket,
    const dxa::protocol::MatchId match,
    const dxa::protocol::PlayerId player,
    const std::chrono::steady_clock::time_point now)
{
    const auto found = tickets_.find(ticket);
    if (found == tickets_.end())
    {
        return TicketConsumeResult::NotFound;
    }
    if (now >= found->second.expiresAt)
    {
        tickets_.erase(found);
        return TicketConsumeResult::Expired;
    }
    if (found->second.match != match || found->second.player != player)
    {
        return TicketConsumeResult::Mismatch;
    }
    tickets_.erase(found);
    return TicketConsumeResult::Accepted;
}

void MatchTicketRegistry::Revoke(
    const std::span<const MatchTicketValue> tickets) noexcept
{
    for (const MatchTicketValue& ticket : tickets)
    {
        tickets_.erase(ticket);
    }
}

void MatchTicketRegistry::PurgeExpired(
    const std::chrono::steady_clock::time_point now) noexcept
{
    for (auto iterator = tickets_.begin(); iterator != tickets_.end();)
    {
        if (now >= iterator->second.expiresAt)
        {
            iterator = tickets_.erase(iterator);
        }
        else
        {
            ++iterator;
        }
    }
}
} // namespace dxa::lobby
