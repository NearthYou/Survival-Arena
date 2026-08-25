#include <dxa/client/LobbyHostFlow.hpp>

#include <dxa/protocol/LobbyMessages.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace
{
using dxa::client::HostCommand;
using dxa::client::LobbyHostFlow;
using dxa::protocol::PlayerId;
using dxa::protocol::RoomMemberView;
using dxa::protocol::RoomSnapshot;
using dxa::protocol::RoomState;

[[nodiscard]] RoomSnapshot Room(
    const RoomState state,
    const PlayerId host,
    const std::initializer_list<RoomMemberView> members)
{
    return RoomSnapshot{
        1U,
        dxa::protocol::RoomId{7U},
        state,
        host,
        std::vector<RoomMemberView>{members}};
}

[[nodiscard]] RoomSnapshot WaitingRoom(
    const PlayerId host,
    const std::initializer_list<RoomMemberView> members)
{
    return Room(RoomState::Waiting, host, members);
}

[[nodiscard]] dxa::protocol::MatchTicket Ticket()
{
    dxa::protocol::MatchTicket ticket;
    ticket.requestId = 5U;
    ticket.match = dxa::protocol::MatchId{9U};
    ticket.host = "127.0.0.1";
    ticket.tcpPort = 7100U;
    ticket.udpPort = 7101U;
    return ticket;
}
} // namespace

TEST(LobbyHostFlow, StartsExactlyOnceWhenExpectedReadyPlayersArrive)
{
    LobbyHostFlow flow{2U};
    EXPECT_EQ(HostCommand::CreateRoom, flow.OnWelcome(PlayerId{4U}));
    EXPECT_EQ(HostCommand::SetReady, flow.OnRoomSnapshot(
        WaitingRoom(PlayerId{4U}, {{PlayerId{4U}, false}})));
    EXPECT_EQ(HostCommand::None, flow.OnRoomSnapshot(
        WaitingRoom(PlayerId{4U}, {{PlayerId{4U}, true}})));
    EXPECT_EQ(HostCommand::StartMatch, flow.OnRoomSnapshot(
        WaitingRoom(
            PlayerId{4U},
            {{PlayerId{4U}, true}, {PlayerId{8U}, true}})));
    EXPECT_EQ(HostCommand::None, flow.OnRoomSnapshot(
        WaitingRoom(
            PlayerId{4U},
            {{PlayerId{4U}, true}, {PlayerId{8U}, true}})));
}

TEST(LobbyHostFlow, AcceptsTwoAndTwentyFourPlayerBoundaries)
{
    EXPECT_NO_THROW((void)LobbyHostFlow{2U});
    EXPECT_NO_THROW((void)LobbyHostFlow{24U});
    EXPECT_THROW((void)LobbyHostFlow{1U}, std::invalid_argument);
    EXPECT_THROW((void)LobbyHostFlow{25U}, std::invalid_argument);
}

TEST(LobbyHostFlow, RejectsNonHostAndExcessMemberSnapshots)
{
    LobbyHostFlow nonHost{2U};
    static_cast<void>(nonHost.OnWelcome(PlayerId{4U}));
    EXPECT_THROW(
        (void)nonHost.OnRoomSnapshot(WaitingRoom(
            PlayerId{8U},
            {{PlayerId{4U}, false}, {PlayerId{8U}, false}})),
        std::logic_error);

    LobbyHostFlow excess{2U};
    static_cast<void>(excess.OnWelcome(PlayerId{4U}));
    EXPECT_THROW(
        (void)excess.OnRoomSnapshot(WaitingRoom(
            PlayerId{4U},
            {
                {PlayerId{4U}, false},
                {PlayerId{8U}, false},
                {PlayerId{12U}, false},
            })),
        std::logic_error);
}

TEST(LobbyHostFlow, WaitsForUnreadyGuestAndIgnoresStartingAndInMatch)
{
    LobbyHostFlow flow{2U};
    static_cast<void>(flow.OnWelcome(PlayerId{4U}));
    static_cast<void>(flow.OnRoomSnapshot(
        WaitingRoom(PlayerId{4U}, {{PlayerId{4U}, false}})));
    EXPECT_EQ(HostCommand::None, flow.OnRoomSnapshot(WaitingRoom(
        PlayerId{4U},
        {{PlayerId{4U}, true}, {PlayerId{8U}, false}})));
    EXPECT_EQ(HostCommand::StartMatch, flow.OnRoomSnapshot(WaitingRoom(
        PlayerId{4U},
        {{PlayerId{4U}, true}, {PlayerId{8U}, true}})));
    EXPECT_EQ(HostCommand::None, flow.OnRoomSnapshot(Room(
        RoomState::Starting,
        PlayerId{4U},
        {{PlayerId{4U}, true}, {PlayerId{8U}, true}})));
    EXPECT_EQ(HostCommand::None, flow.OnRoomSnapshot(Room(
        RoomState::InMatch,
        PlayerId{4U},
        {{PlayerId{4U}, true}, {PlayerId{8U}, true}})));
}

TEST(LobbyHostFlow, LobbyErrorIsTerminal)
{
    LobbyHostFlow flow{2U};
    static_cast<void>(flow.OnWelcome(PlayerId{4U}));

    flow.OnError(dxa::protocol::LobbyError::WorkerUnavailable);

    EXPECT_TRUE(flow.Terminal());
    EXPECT_EQ(
        dxa::protocol::LobbyError::WorkerUnavailable,
        flow.Error());
    EXPECT_EQ(HostCommand::None, flow.OnRoomSnapshot(
        WaitingRoom(PlayerId{4U}, {{PlayerId{4U}, false}})));
}

TEST(LobbyHostFlow, RejectsTicketBeforeWelcomeAndDuplicateTicket)
{
    LobbyHostFlow early{2U};
    EXPECT_THROW(early.OnMatchTicket(Ticket()), std::logic_error);

    LobbyHostFlow flow{2U};
    static_cast<void>(flow.OnWelcome(PlayerId{4U}));
    static_cast<void>(flow.OnRoomSnapshot(
        WaitingRoom(PlayerId{4U}, {{PlayerId{4U}, false}})));
    static_cast<void>(flow.OnRoomSnapshot(WaitingRoom(
        PlayerId{4U},
        {{PlayerId{4U}, true}, {PlayerId{8U}, true}})));
    EXPECT_NO_THROW(flow.OnMatchTicket(Ticket()));
    EXPECT_TRUE(flow.TicketReceived());
    EXPECT_THROW(flow.OnMatchTicket(Ticket()), std::logic_error);
}
