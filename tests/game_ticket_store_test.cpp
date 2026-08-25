#include <dxa/game_server/GameTicketStore.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace
{
using namespace std::chrono_literals;
using dxa::game_server::GameTicketConsumeResult;
using dxa::game_server::GameTicketStore;
using dxa::protocol::MatchId;
using dxa::protocol::MatchTicketValue;
using dxa::protocol::PlayerId;
using dxa::protocol::ReservedParticipant;

[[nodiscard]] MatchTicketValue Ticket(const std::uint8_t seed)
{
    MatchTicketValue ticket;
    for (std::size_t index = 0U; index < ticket.size(); ++index)
    {
        ticket[index] = static_cast<std::byte>(
            static_cast<std::uint8_t>(seed + index));
    }
    return ticket;
}

[[nodiscard]] std::chrono::steady_clock::time_point TimeMs(
    const std::int64_t milliseconds)
{
    return std::chrono::steady_clock::time_point{
        std::chrono::milliseconds{milliseconds}};
}

[[nodiscard]] std::vector<ReservedParticipant> Participants(
    const std::uint32_t count)
{
    std::vector<ReservedParticipant> participants;
    participants.reserve(count);
    for (std::uint32_t index = 0U; index < count; ++index)
    {
        participants.push_back({
            PlayerId{index + 1U},
            Ticket(static_cast<std::uint8_t>(index + 1U))});
    }
    return participants;
}
} // namespace

TEST(GameTicketStore, AcceptsExactTicketOnceAndKeepsMismatchUnconsumed)
{
    GameTicketStore store;
    const std::vector<ReservedParticipant> participants{
        {PlayerId{2U}, Ticket(1U)},
        {PlayerId{9U}, Ticket(2U)}};
    store.Load(MatchId{7U}, participants, TimeMs(0), std::chrono::seconds{60});

    EXPECT_EQ(
        GameTicketConsumeResult::Mismatch,
        store.Consume(Ticket(1U), MatchId{7U}, PlayerId{9U}, TimeMs(1000)));
    EXPECT_EQ(
        GameTicketConsumeResult::Mismatch,
        store.Consume(Ticket(1U), MatchId{8U}, PlayerId{2U}, TimeMs(1000)));
    EXPECT_EQ(
        GameTicketConsumeResult::Accepted,
        store.Consume(Ticket(1U), MatchId{7U}, PlayerId{2U}, TimeMs(1000)));
    EXPECT_EQ(
        GameTicketConsumeResult::Used,
        store.Consume(Ticket(1U), MatchId{7U}, PlayerId{2U}, TimeMs(1000)));
    EXPECT_EQ(
        GameTicketConsumeResult::NotFound,
        store.Consume(Ticket(99U), MatchId{7U}, PlayerId{2U}, TimeMs(1000)));
}

TEST(GameTicketStore, ExpiresAtExactLifetimeBoundary)
{
    GameTicketStore store;
    const std::vector<ReservedParticipant> participants{
        {PlayerId{1U}, Ticket(3U)},
        {PlayerId{2U}, Ticket(4U)}};
    store.Load(MatchId{1U}, participants, TimeMs(0), std::chrono::seconds{60});

    EXPECT_EQ(
        GameTicketConsumeResult::Accepted,
        store.Consume(
            Ticket(3U), MatchId{1U}, PlayerId{1U}, TimeMs(59999)));
    EXPECT_EQ(
        GameTicketConsumeResult::Expired,
        store.Consume(
            Ticket(4U), MatchId{1U}, PlayerId{2U}, TimeMs(60000)));
    EXPECT_EQ(
        GameTicketConsumeResult::NotFound,
        store.Consume(
            Ticket(4U), MatchId{1U}, PlayerId{2U}, TimeMs(60000)));
    EXPECT_EQ(
        GameTicketConsumeResult::Used,
        store.Consume(
            Ticket(3U), MatchId{1U}, PlayerId{1U}, TimeMs(60000)));
}

TEST(GameTicketStore, PurgeReturnsExpiredPlayersInNumericOrder)
{
    GameTicketStore store;
    const std::vector<ReservedParticipant> participants{
        {PlayerId{9U}, Ticket(9U)},
        {PlayerId{2U}, Ticket(2U)},
        {PlayerId{5U}, Ticket(5U)}};
    store.Load(MatchId{3U}, participants, TimeMs(100), 500ms);

    EXPECT_TRUE(store.PurgeExpired(TimeMs(599)).empty());
    EXPECT_EQ(
        (std::vector<PlayerId>{PlayerId{2U}, PlayerId{5U}, PlayerId{9U}}),
        store.PurgeExpired(TimeMs(600)));
    EXPECT_TRUE(store.PurgeExpired(TimeMs(600)).empty());
}

TEST(GameTicketStore, RejectsParticipantAndLifetimeBounds)
{
    GameTicketStore tooFew;
    EXPECT_THROW(
        tooFew.Load(MatchId{1U}, Participants(1U), TimeMs(0), 1ms),
        std::invalid_argument);

    GameTicketStore tooMany;
    EXPECT_THROW(
        tooMany.Load(MatchId{1U}, Participants(25U), TimeMs(0), 1ms),
        std::invalid_argument);

    GameTicketStore zeroLifetime;
    EXPECT_THROW(
        zeroLifetime.Load(
            MatchId{1U}, Participants(2U), TimeMs(0), 0ms),
        std::invalid_argument);

    GameTicketStore minimum;
    minimum.Load(MatchId{1U}, Participants(2U), TimeMs(0), 1ms);
    EXPECT_EQ(
        GameTicketConsumeResult::Accepted,
        minimum.Consume(
            Ticket(1U), MatchId{1U}, PlayerId{1U}, TimeMs(0)));

    GameTicketStore maximum;
    maximum.Load(MatchId{2U}, Participants(24U), TimeMs(0), 1ms);
    EXPECT_EQ(
        GameTicketConsumeResult::Accepted,
        maximum.Consume(
            Ticket(24U), MatchId{2U}, PlayerId{24U}, TimeMs(0)));
}

TEST(GameTicketStore, RejectsDuplicatePlayersAndTicketsWithoutPartialLoad)
{
    GameTicketStore duplicatePlayer;
    const std::vector<ReservedParticipant> players{
        {PlayerId{1U}, Ticket(1U)},
        {PlayerId{1U}, Ticket(2U)}};
    EXPECT_THROW(
        duplicatePlayer.Load(MatchId{1U}, players, TimeMs(0), 1s),
        std::invalid_argument);
    EXPECT_EQ(
        GameTicketConsumeResult::NotFound,
        duplicatePlayer.Consume(
            Ticket(1U), MatchId{1U}, PlayerId{1U}, TimeMs(0)));

    GameTicketStore duplicateTicket;
    const std::vector<ReservedParticipant> tickets{
        {PlayerId{1U}, Ticket(1U)},
        {PlayerId{2U}, Ticket(1U)}};
    EXPECT_THROW(
        duplicateTicket.Load(MatchId{1U}, tickets, TimeMs(0), 1s),
        std::invalid_argument);
    EXPECT_EQ(
        GameTicketConsumeResult::NotFound,
        duplicateTicket.Consume(
            Ticket(1U), MatchId{1U}, PlayerId{1U}, TimeMs(0)));
}

TEST(GameTicketStore, RejectsSecondLoadAndRetainsUsedValuesAcrossPurge)
{
    GameTicketStore store;
    store.Load(MatchId{1U}, Participants(2U), TimeMs(0), 1s);
    ASSERT_EQ(
        GameTicketConsumeResult::Accepted,
        store.Consume(
            Ticket(1U), MatchId{1U}, PlayerId{1U}, TimeMs(0)));

    EXPECT_THROW(
        store.Load(MatchId{2U}, Participants(2U), TimeMs(0), 1s),
        std::logic_error);
    EXPECT_EQ(
        (std::vector<PlayerId>{PlayerId{2U}}),
        store.PurgeExpired(TimeMs(1000)));
    EXPECT_EQ(
        GameTicketConsumeResult::Used,
        store.Consume(
            Ticket(1U), MatchId{1U}, PlayerId{1U}, TimeMs(1000)));
}
