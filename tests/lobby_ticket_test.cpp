#include <dxa/lobby/MatchTicketRegistry.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <optional>
#include <vector>

namespace
{
using dxa::lobby::ITicketSource;
using dxa::lobby::MatchTicketRegistry;
using dxa::protocol::MatchTicketValue;
using dxa::lobby::TicketConsumeResult;
using dxa::protocol::MatchId;
using dxa::protocol::PlayerId;

[[nodiscard]] MatchTicketValue Ticket(const std::uint8_t seed)
{
    MatchTicketValue value{};
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        value[index] = static_cast<std::byte>(
            static_cast<std::uint8_t>(seed + index));
    }
    return value;
}

[[nodiscard]] std::chrono::steady_clock::time_point Time(
    const std::int64_t seconds)
{
    return std::chrono::steady_clock::time_point{
        std::chrono::seconds{seconds}};
}

class SequenceTicketSource final : public ITicketSource
{
public:
    explicit SequenceTicketSource(
        const std::initializer_list<std::optional<MatchTicketValue>> sequence)
        : sequence_{sequence}
    {
    }

    [[nodiscard]] bool Fill(
        const std::span<std::byte, dxa::protocol::MatchTicketBytes> output) noexcept override
    {
        if (sequence_.empty())
        {
            return false;
        }
        const std::optional<MatchTicketValue> next = sequence_.front();
        sequence_.pop_front();
        if (!next.has_value())
        {
            return false;
        }
        std::copy(next->begin(), next->end(), output.begin());
        return true;
    }

private:
    std::deque<std::optional<MatchTicketValue>> sequence_;
};
} // namespace

TEST(MatchTicketRegistry, IssuesDistinctTicketsAndConsumesOnce)
{
    SequenceTicketSource source{{Ticket(1U), Ticket(2U)}};
    MatchTicketRegistry registry{source};

    const auto first = registry.Issue(MatchId{7U}, PlayerId{1U}, Time(0));
    const auto second = registry.Issue(MatchId{7U}, PlayerId{2U}, Time(0));

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_NE(*first, *second);
    EXPECT_EQ(
        TicketConsumeResult::Accepted,
        registry.Consume(*first, MatchId{7U}, PlayerId{1U}, Time(0)));
    EXPECT_EQ(
        TicketConsumeResult::NotFound,
        registry.Consume(*first, MatchId{7U}, PlayerId{1U}, Time(0)));
}

TEST(MatchTicketRegistry, ExpiresAtExactlySixtySeconds)
{
    SequenceTicketSource source{{Ticket(3U), Ticket(4U)}};
    MatchTicketRegistry registry{source};
    const auto beforeBoundary = registry.Issue(MatchId{1U}, PlayerId{1U}, Time(0));
    const auto atBoundary = registry.Issue(MatchId{1U}, PlayerId{2U}, Time(0));
    ASSERT_TRUE(beforeBoundary.has_value());
    ASSERT_TRUE(atBoundary.has_value());

    EXPECT_EQ(
        TicketConsumeResult::Accepted,
        registry.Consume(*beforeBoundary, MatchId{1U}, PlayerId{1U}, Time(59)));
    EXPECT_EQ(
        TicketConsumeResult::Expired,
        registry.Consume(*atBoundary, MatchId{1U}, PlayerId{2U}, Time(60)));
    EXPECT_EQ(
        TicketConsumeResult::NotFound,
        registry.Consume(*atBoundary, MatchId{1U}, PlayerId{2U}, Time(60)));
}

TEST(MatchTicketRegistry, MismatchDoesNotConsumeTicket)
{
    SequenceTicketSource source{{Ticket(5U)}};
    MatchTicketRegistry registry{source};
    const auto ticket = registry.Issue(MatchId{2U}, PlayerId{4U}, Time(0));
    ASSERT_TRUE(ticket.has_value());

    EXPECT_EQ(
        TicketConsumeResult::Mismatch,
        registry.Consume(*ticket, MatchId{2U}, PlayerId{5U}, Time(0)));
    EXPECT_EQ(
        TicketConsumeResult::Mismatch,
        registry.Consume(*ticket, MatchId{3U}, PlayerId{4U}, Time(0)));
    EXPECT_EQ(
        TicketConsumeResult::Accepted,
        registry.Consume(*ticket, MatchId{2U}, PlayerId{4U}, Time(0)));
}

TEST(MatchTicketRegistry, RejectsEntropyFailureAndEightCollisions)
{
    SequenceTicketSource failedSource{{std::nullopt}};
    MatchTicketRegistry failed{failedSource};
    EXPECT_FALSE(failed.Issue(MatchId{1U}, PlayerId{1U}, Time(0)).has_value());

    const MatchTicketValue duplicate = Ticket(7U);
    SequenceTicketSource collisionSource{{
        duplicate,
        duplicate,
        duplicate,
        duplicate,
        duplicate,
        duplicate,
        duplicate,
        duplicate,
        duplicate}};
    MatchTicketRegistry collisions{collisionSource};
    ASSERT_TRUE(collisions.Issue(MatchId{1U}, PlayerId{1U}, Time(0)).has_value());
    EXPECT_FALSE(collisions.Issue(MatchId{1U}, PlayerId{2U}, Time(0)).has_value());
}

TEST(MatchTicketRegistry, RevokesTransactionTickets)
{
    SequenceTicketSource source{{Ticket(8U), Ticket(9U)}};
    MatchTicketRegistry registry{source};
    const auto first = registry.Issue(MatchId{5U}, PlayerId{1U}, Time(0));
    const auto second = registry.Issue(MatchId{5U}, PlayerId{2U}, Time(0));
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());

    const std::array tickets{*first, *second};
    registry.Revoke(tickets);

    EXPECT_EQ(
        TicketConsumeResult::NotFound,
        registry.Consume(*first, MatchId{5U}, PlayerId{1U}, Time(0)));
    EXPECT_EQ(
        TicketConsumeResult::NotFound,
        registry.Consume(*second, MatchId{5U}, PlayerId{2U}, Time(0)));
}

TEST(MatchTicketRegistry, PurgesExpiredRecordsOnly)
{
    SequenceTicketSource source{{Ticket(10U), Ticket(11U)}};
    MatchTicketRegistry registry{source};
    const auto expired = registry.Issue(MatchId{1U}, PlayerId{1U}, Time(0));
    const auto live = registry.Issue(MatchId{1U}, PlayerId{2U}, Time(30));
    ASSERT_TRUE(expired.has_value());
    ASSERT_TRUE(live.has_value());

    registry.PurgeExpired(Time(60));

    EXPECT_EQ(
        TicketConsumeResult::NotFound,
        registry.Consume(*expired, MatchId{1U}, PlayerId{1U}, Time(60)));
    EXPECT_EQ(
        TicketConsumeResult::Accepted,
        registry.Consume(*live, MatchId{1U}, PlayerId{2U}, Time(60)));
}
