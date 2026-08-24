#include "support/game_network_fixture.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

TEST(GameServerIntegration, CompletesLobbyReservationAuthenticationAndDisconnectResult)
{
    dxa::test::GameNetworkFixture fixture;
    fixture.StartLobbyAndWorker();
    const dxa::test::ReadyNetworkRoom room =
        fixture.CreateReadyTwoPlayerRoom();
    fixture.StartMatch(room.host);
    const dxa::test::TicketPair tickets = fixture.WaitForTwoTickets(room);

    const auto host = fixture.Authenticate(tickets.host);
    const auto guest = fixture.Authenticate(tickets.guest);
    ASSERT_NE(nullptr, host->Welcome());
    ASSERT_NE(nullptr, guest->Welcome());
    host->BindUdp();
    guest->BindUdp();
    host->SendDestination({0.0F, 0.0F}, 1U);
    fixture.WaitForSnapshotAck(host, 1U);
    guest->CloseTcp();

    const dxa::protocol::GameMatchResult result = fixture.WaitForResult(host);
    EXPECT_TRUE(result.hasWinner);
    EXPECT_EQ(host->Actor(), result.winner);
    fixture.WaitForRoomCleanup(room);
}

TEST(GameServerIntegration, RejectsInvalidWrongPlayerAndReusedTickets)
{
    dxa::test::GameNetworkFixture fixture;
    fixture.StartLobbyAndWorker();
    const dxa::test::ReadyNetworkRoom room =
        fixture.CreateReadyTwoPlayerRoom();
    fixture.StartMatch(room.host);
    const dxa::test::TicketPair tickets = fixture.WaitForTwoTickets(room);

    dxa::test::GameTicketCredential invalid = tickets.host;
    invalid.ticket.ticket[0] = static_cast<std::byte>(
        std::to_integer<std::uint8_t>(invalid.ticket.ticket[0]) ^ 0xFFU);
    const auto invalidClient = fixture.Authenticate(invalid);
    ASSERT_NE(nullptr, invalidClient->Error());
    EXPECT_EQ(
        dxa::protocol::GameServerErrorCode::AuthenticationFailed,
        invalidClient->Error()->error);

    dxa::test::GameTicketCredential wrongPlayer = tickets.host;
    wrongPlayer.player = tickets.guest.player;
    const auto wrongClient = fixture.Authenticate(wrongPlayer);
    ASSERT_NE(nullptr, wrongClient->Error());
    EXPECT_EQ(
        dxa::protocol::GameServerErrorCode::AuthenticationFailed,
        wrongClient->Error()->error);

    const auto host = fixture.Authenticate(tickets.host);
    ASSERT_NE(nullptr, host->Welcome());
    const auto reused = fixture.Authenticate(tickets.host);
    ASSERT_NE(nullptr, reused->Error());
    EXPECT_EQ(
        dxa::protocol::GameServerErrorCode::AuthenticationFailed,
        reused->Error()->error);

    const auto guest = fixture.Authenticate(tickets.guest);
    ASSERT_NE(nullptr, guest->Welcome());
    guest->CloseTcp();
    static_cast<void>(fixture.WaitForResult(host));
    fixture.WaitForRoomCleanup(room);
}

TEST(GameServerIntegration, RejectsBadTokenRebindAndOldInputWithoutStealingBinding)
{
    dxa::test::GameNetworkFixture fixture;
    fixture.StartLobbyAndWorker();
    const dxa::test::ReadyNetworkRoom room =
        fixture.CreateReadyTwoPlayerRoom();
    fixture.StartMatch(room.host);
    const dxa::test::TicketPair tickets = fixture.WaitForTwoTickets(room);
    const auto host = fixture.Authenticate(tickets.host);
    const auto guest = fixture.Authenticate(tickets.guest);
    ASSERT_NE(nullptr, host->Welcome());
    ASSERT_NE(nullptr, guest->Welcome());
    host->BindUdp();
    guest->BindUdp();

    dxa::protocol::UdpSessionToken wrong = host->Welcome()->udpToken;
    wrong[0] = static_cast<std::byte>(
        std::to_integer<std::uint8_t>(wrong[0]) ^ 0xFFU);
    host->SendRogueBind(wrong);
    host->SendRogueBind(host->Welcome()->udpToken);
    host->SendDestination({0.0F, 0.0F}, 2U);
    host->SendDestination({20.0F, 20.0F}, 1U);
    host->SendDestination({20.0F, 20.0F}, 2U);

    fixture.WaitForSnapshotAck(host, 2U);
    guest->CloseTcp();
    static_cast<void>(fixture.WaitForResult(host));
    fixture.WaitForRoomCleanup(room);
}

TEST(GameServerIntegration, ActiveWorkerControlCloseCleansLobbyRoom)
{
    dxa::test::GameNetworkFixture fixture;
    fixture.StartLobbyAndWorker();
    const dxa::test::ReadyNetworkRoom room =
        fixture.CreateReadyTwoPlayerRoom();
    fixture.StartMatch(room.host);
    static_cast<void>(fixture.WaitForTwoTickets(room));

    fixture.StopWorker();
    fixture.RunUntil([&room] {
        const auto* error =
            dxa::test::LatestLobbyMessage<dxa::protocol::ErrorResponse>(
                *room.host);
        const auto* rooms =
            dxa::test::LatestLobbyMessage<dxa::protocol::RoomListResponse>(
                *room.host);
        return error != nullptr
            && error->error == dxa::protocol::LobbyError::MatchUnavailable
            && rooms != nullptr
            && rooms->rooms.empty();
    });
}

TEST(GameServerIntegration, ExpiresUnauthenticatedTicketsThroughLiveMatchTimer)
{
    dxa::test::ExpiringGameWorkerFixture fixture;

    const dxa::protocol::MatchFinished finished = fixture.WaitForCompletion();

    EXPECT_EQ(
        dxa::protocol::MatchCompletionReason::NoAuthenticatedPlayers,
        finished.reason);
    EXPECT_FALSE(finished.hasWinner);
    EXPECT_EQ(0U, finished.finishedTick);
}

TEST(GameServerIntegration, CompletesThreeSequentialReservationsOnOneWorker)
{
    dxa::test::GameNetworkFixture fixture;
    fixture.StartLobbyAndWorker();
    for (std::uint32_t match = 0U; match < 3U; ++match)
    {
        static_cast<void>(match);
        const dxa::test::ReadyNetworkRoom room =
            fixture.CreateReadyTwoPlayerRoom();
        fixture.StartMatch(room.host);
        const dxa::test::TicketPair tickets = fixture.WaitForTwoTickets(room);
        const auto host = fixture.Authenticate(tickets.host);
        const auto guest = fixture.Authenticate(tickets.guest);
        ASSERT_NE(nullptr, host->Welcome());
        ASSERT_NE(nullptr, guest->Welcome());
        guest->CloseTcp();
        static_cast<void>(fixture.WaitForResult(host));
        fixture.WaitForRoomCleanup(room);
    }
}
