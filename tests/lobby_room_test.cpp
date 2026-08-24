#include <dxa/lobby/Room.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>

namespace
{
using dxa::lobby::Room;
using dxa::protocol::LobbyError;
using dxa::protocol::PlayerId;
using dxa::protocol::RoomId;
using dxa::protocol::RoomState;

[[nodiscard]] Room RoomWithPlayers(const std::uint32_t count)
{
    Room room = Room::Create(RoomId{1U}, PlayerId{1U}, 1U);
    for (std::uint32_t value = 2U; value <= count; ++value)
    {
        const auto error = room.Join(PlayerId{value}, value);
        if (error.has_value())
        {
            throw std::logic_error{"test room setup failed"};
        }
    }
    return room;
}
} // namespace

TEST(LobbyRoom, CreatesWaitingRoomWithUnreadyHost)
{
    const Room room = Room::Create(RoomId{7U}, PlayerId{11U}, 99U);

    const auto snapshot = room.Snapshot(5U);
    EXPECT_EQ(RoomId{7U}, snapshot.room);
    EXPECT_EQ(RoomState::Waiting, snapshot.state);
    EXPECT_EQ(PlayerId{11U}, snapshot.host);
    ASSERT_EQ(1U, snapshot.members.size());
    EXPECT_EQ(PlayerId{11U}, snapshot.members[0].player);
    EXPECT_FALSE(snapshot.members[0].ready);
    EXPECT_EQ(5U, snapshot.requestId);
}

TEST(LobbyRoom, AcceptsTwentyFourthAndRejectsTwentyFifthPlayer)
{
    Room room = RoomWithPlayers(23U);

    EXPECT_FALSE(room.Join(PlayerId{24U}, 24U).has_value());
    EXPECT_EQ(24U, room.Players().size());
    EXPECT_EQ(LobbyError::RoomFull, room.Join(PlayerId{25U}, 25U));
    EXPECT_EQ(24U, room.Players().size());
}

TEST(LobbyRoom, RejectsDuplicateAndNonWaitingJoin)
{
    Room room = RoomWithPlayers(2U);
    EXPECT_EQ(LobbyError::AlreadyInRoom, room.Join(PlayerId{2U}, 3U));

    ASSERT_FALSE(room.SetReady(PlayerId{1U}, true).has_value());
    ASSERT_FALSE(room.SetReady(PlayerId{2U}, true).has_value());
    ASSERT_FALSE(room.ValidateStart(PlayerId{1U}).has_value());
    room.BeginStarting();

    EXPECT_EQ(LobbyError::RoomNotJoinable, room.Join(PlayerId{3U}, 4U));
}

TEST(LobbyRoom, TransfersHostToEarliestRemainingJoin)
{
    Room room = Room::Create(RoomId{1U}, PlayerId{10U}, 100U);
    ASSERT_FALSE(room.Join(PlayerId{30U}, 102U).has_value());
    ASSERT_FALSE(room.Join(PlayerId{20U}, 101U).has_value());

    ASSERT_FALSE(room.Leave(PlayerId{10U}).has_value());

    EXPECT_EQ(PlayerId{20U}, room.Host());
    EXPECT_EQ(LobbyError::NotInRoom, room.Leave(PlayerId{99U}));
}

TEST(LobbyRoom, DeletesLastMemberIntoEmptyState)
{
    Room room = Room::Create(RoomId{1U}, PlayerId{1U}, 1U);

    ASSERT_FALSE(room.Leave(PlayerId{1U}).has_value());

    EXPECT_TRUE(room.Empty());
    EXPECT_THROW((void)room.Host(), std::logic_error);
}

TEST(LobbyRoom, RequiresHostTwoPlayersAndEveryMemberReady)
{
    Room room = Room::Create(RoomId{1U}, PlayerId{1U}, 1U);
    EXPECT_EQ(
        LobbyError::MinimumPlayersRequired,
        room.ValidateStart(PlayerId{1U}));

    ASSERT_FALSE(room.Join(PlayerId{2U}, 2U).has_value());
    EXPECT_EQ(LobbyError::NotHost, room.ValidateStart(PlayerId{2U}));
    EXPECT_EQ(LobbyError::NotAllReady, room.ValidateStart(PlayerId{1U}));

    ASSERT_FALSE(room.SetReady(PlayerId{1U}, true).has_value());
    EXPECT_EQ(LobbyError::NotAllReady, room.ValidateStart(PlayerId{1U}));
    ASSERT_FALSE(room.SetReady(PlayerId{2U}, true).has_value());
    EXPECT_FALSE(room.ValidateStart(PlayerId{1U}).has_value());
}

TEST(LobbyRoom, PreservesReadyValuesAcrossStartingRollback)
{
    Room room = RoomWithPlayers(2U);
    ASSERT_FALSE(room.SetReady(PlayerId{1U}, true).has_value());
    ASSERT_FALSE(room.SetReady(PlayerId{2U}, true).has_value());
    room.BeginStarting();

    room.ReturnToWaiting();

    const auto snapshot = room.Snapshot(0U);
    EXPECT_EQ(RoomState::Waiting, snapshot.state);
    EXPECT_TRUE(snapshot.members[0].ready);
    EXPECT_TRUE(snapshot.members[1].ready);
}

TEST(LobbyRoom, MarksInMatchAndRejectsWaitingMutations)
{
    Room room = RoomWithPlayers(2U);
    ASSERT_FALSE(room.SetReady(PlayerId{1U}, true).has_value());
    ASSERT_FALSE(room.SetReady(PlayerId{2U}, true).has_value());
    room.BeginStarting();
    room.MarkInMatch();

    EXPECT_EQ(RoomState::InMatch, room.Snapshot(0U).state);
    EXPECT_EQ(LobbyError::RoomNotJoinable, room.SetReady(PlayerId{1U}, false));
    EXPECT_EQ(LobbyError::RoomNotJoinable, room.Leave(PlayerId{1U}));
    EXPECT_EQ(LobbyError::RoomNotJoinable, room.ValidateStart(PlayerId{1U}));
}

TEST(LobbyRoom, SortsPlayersAndSnapshotMembersByPlayerId)
{
    Room room = Room::Create(RoomId{1U}, PlayerId{9U}, 1U);
    ASSERT_FALSE(room.Join(PlayerId{3U}, 2U).has_value());
    ASSERT_FALSE(room.Join(PlayerId{7U}, 3U).has_value());

    const auto players = room.Players();
    ASSERT_EQ(3U, players.size());
    EXPECT_EQ(PlayerId{3U}, players[0]);
    EXPECT_EQ(PlayerId{7U}, players[1]);
    EXPECT_EQ(PlayerId{9U}, players[2]);

    const auto snapshot = room.Snapshot(0U);
    ASSERT_EQ(3U, snapshot.members.size());
    EXPECT_EQ(PlayerId{3U}, snapshot.members[0].player);
    EXPECT_EQ(PlayerId{7U}, snapshot.members[1].player);
    EXPECT_EQ(PlayerId{9U}, snapshot.members[2].player);
}

TEST(LobbyRoom, RejectsInvalidTransitionCalls)
{
    Room unready = RoomWithPlayers(2U);
    EXPECT_THROW(unready.BeginStarting(), std::logic_error);

    Room waiting = RoomWithPlayers(2U);
    EXPECT_THROW(waiting.ReturnToWaiting(), std::logic_error);
    EXPECT_THROW(waiting.MarkInMatch(), std::logic_error);

    ASSERT_FALSE(waiting.SetReady(PlayerId{1U}, true).has_value());
    ASSERT_FALSE(waiting.SetReady(PlayerId{2U}, true).has_value());
    waiting.BeginStarting();
    EXPECT_THROW(waiting.BeginStarting(), std::logic_error);
}
