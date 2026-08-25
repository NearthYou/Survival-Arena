#include <dxa/game_server/ParticipantRoster.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace
{
using dxa::game_server::GameConnectionId;
using dxa::game_server::ParticipantRoster;
using dxa::protocol::EntityId;
using dxa::protocol::PlayerId;
using dxa::protocol::UdpSessionToken;

[[nodiscard]] UdpSessionToken Token(const std::uint8_t seed)
{
    UdpSessionToken token;
    for (std::size_t index = 0U; index < token.size(); ++index)
    {
        token[index] = static_cast<std::byte>(
            static_cast<std::uint8_t>(seed + index));
    }
    return token;
}

[[nodiscard]] std::vector<PlayerId> Players(const std::uint32_t count)
{
    std::vector<PlayerId> players;
    players.reserve(count);
    for (std::uint32_t index = 0U; index < count; ++index)
    {
        players.push_back(PlayerId{index + 1U});
    }
    return players;
}
} // namespace

TEST(ParticipantRoster, MapsSortedPlayersToZeroBasedActors)
{
    ParticipantRoster roster{{PlayerId{9U}, PlayerId{2U}, PlayerId{5U}}};

    EXPECT_EQ(EntityId{0U}, roster.ActorFor(PlayerId{2U}));
    EXPECT_EQ(EntityId{1U}, roster.ActorFor(PlayerId{5U}));
    EXPECT_EQ(EntityId{2U}, roster.ActorFor(PlayerId{9U}));
    EXPECT_EQ(PlayerId{2U}, roster.PlayerFor(EntityId{0U}));
    EXPECT_EQ(PlayerId{9U}, roster.PlayerFor(EntityId{2U}));
    EXPECT_FALSE(roster.ReadyToStart());
    EXPECT_THROW((void)roster.ActorFor(PlayerId{7U}), std::out_of_range);
    EXPECT_THROW((void)roster.PlayerFor(EntityId{3U}), std::out_of_range);
}

TEST(ParticipantRoster, AcceptsTwoAndTwentyFourPlayerBounds)
{
    ParticipantRoster minimum{Players(2U)};
    ParticipantRoster maximum{Players(24U)};

    EXPECT_EQ(EntityId{1U}, minimum.ActorFor(PlayerId{2U}));
    EXPECT_EQ(EntityId{23U}, maximum.ActorFor(PlayerId{24U}));
    EXPECT_EQ(PlayerId{24U}, maximum.PlayerFor(EntityId{23U}));
}

TEST(ParticipantRoster, RejectsCountAndDuplicatePlayerBoundaries)
{
    EXPECT_THROW(ParticipantRoster{Players(1U)}, std::invalid_argument);
    EXPECT_THROW(ParticipantRoster{Players(25U)}, std::invalid_argument);
    EXPECT_THROW(
        ParticipantRoster({PlayerId{2U}, PlayerId{2U}}),
        std::invalid_argument);
}

TEST(ParticipantRoster, AuthenticatesPendingSlotAndExposesConnection)
{
    ParticipantRoster roster{{PlayerId{5U}, PlayerId{2U}}};

    EXPECT_TRUE(roster.Authenticate(
        PlayerId{2U}, GameConnectionId{10U}, Token(1U)));
    EXPECT_EQ(1U, roster.AuthenticatedCount());
    EXPECT_EQ(
        GameConnectionId{10U},
        roster.ConnectionFor(PlayerId{2U}));
    EXPECT_FALSE(roster.ConnectionFor(PlayerId{5U}).has_value());
    EXPECT_FALSE(roster.Authenticate(
        PlayerId{2U}, GameConnectionId{11U}, Token(2U)));
    EXPECT_FALSE(roster.Authenticate(
        PlayerId{99U}, GameConnectionId{12U}, Token(3U)));
}

TEST(ParticipantRoster, RejectsDuplicateConnectionWithoutResolvingOtherSlot)
{
    ParticipantRoster roster{{PlayerId{2U}, PlayerId{5U}}};
    ASSERT_TRUE(roster.Authenticate(
        PlayerId{2U}, GameConnectionId{10U}, Token(1U)));

    EXPECT_FALSE(roster.Authenticate(
        PlayerId{5U}, GameConnectionId{10U}, Token(2U)));
    EXPECT_EQ(1U, roster.AuthenticatedCount());
    EXPECT_FALSE(roster.ReadyToStart());
    EXPECT_TRUE(roster.Authenticate(
        PlayerId{5U}, GameConnectionId{11U}, Token(2U)));
    EXPECT_TRUE(roster.ReadyToStart());
}

TEST(ParticipantRoster, PendingAndAuthenticatedSlotsCanBecomeUnavailable)
{
    ParticipantRoster roster{{PlayerId{9U}, PlayerId{2U}, PlayerId{5U}}};
    ASSERT_TRUE(roster.Authenticate(
        PlayerId{2U}, GameConnectionId{10U}, Token(1U)));

    EXPECT_TRUE(roster.MarkUnavailable(PlayerId{2U}));
    EXPECT_FALSE(roster.ConnectionFor(PlayerId{2U}).has_value());
    EXPECT_EQ(0U, roster.AuthenticatedCount());
    EXPECT_TRUE(roster.MarkUnavailable(PlayerId{9U}));
    EXPECT_FALSE(roster.MarkUnavailable(PlayerId{9U}));
    EXPECT_FALSE(roster.MarkUnavailable(PlayerId{99U}));
    EXPECT_EQ(
        (std::vector<PlayerId>{PlayerId{2U}, PlayerId{9U}}),
        roster.UnavailablePlayers());
    EXPECT_FALSE(roster.ReadyToStart());
}

TEST(ParticipantRoster, StartsWhenEverySlotIsAuthenticatedOrUnavailable)
{
    ParticipantRoster roster{{PlayerId{9U}, PlayerId{2U}, PlayerId{5U}}};
    ASSERT_TRUE(roster.Authenticate(
        PlayerId{2U}, GameConnectionId{10U}, Token(1U)));
    ASSERT_TRUE(roster.Authenticate(
        PlayerId{5U}, GameConnectionId{11U}, Token(2U)));
    ASSERT_TRUE(roster.MarkUnavailable(PlayerId{9U}));

    EXPECT_TRUE(roster.ReadyToStart());
    EXPECT_EQ(2U, roster.AuthenticatedCount());
    EXPECT_EQ(
        (std::vector<PlayerId>{PlayerId{9U}}),
        roster.UnavailablePlayers());
    EXPECT_FALSE(roster.Authenticate(
        PlayerId{9U}, GameConnectionId{12U}, Token(3U)));
}
