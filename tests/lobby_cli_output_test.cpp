#include <dxa/lobby_cli/LobbyCliOutput.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

using dxa::lobby_cli::FormatLobbyServerMessage;
using namespace dxa::protocol;

TEST(LobbyCliOutput, RedactsTicketBytes)
{
    MatchTicket ticket;
    ticket.requestId = 7U;
    ticket.match = MatchId{9U};
    ticket.ticket.fill(std::byte{0xAB});
    ticket.host = "127.0.0.1";
    ticket.tcpPort = 7100U;
    ticket.udpPort = 7101U;

    const std::string output = FormatLobbyServerMessage(ServerMessage{ticket});
    EXPECT_NE(std::string::npos, output.find("match ticket received"));
    EXPECT_NE(std::string::npos, output.find("match=9"));
    EXPECT_EQ(std::string::npos, output.find("AB"));
    EXPECT_EQ(std::string::npos, output.find("ab"));
}

TEST(LobbyCliOutput, ShowsPublicWelcomeRoomAndErrorFields)
{
    const std::string welcome = FormatLobbyServerMessage(
        ServerMessage{ServerWelcome{1U, PlayerId{4U}}});
    EXPECT_NE(std::string::npos, welcome.find("player=4"));

    const std::string room = FormatLobbyServerMessage(ServerMessage{RoomSnapshot{
        2U,
        RoomId{3U},
        RoomState::Waiting,
        PlayerId{4U},
        {{PlayerId{4U}, true}, {PlayerId{8U}, false}}}});
    EXPECT_NE(std::string::npos, room.find("room=3"));
    EXPECT_NE(std::string::npos, room.find("host=4"));
    EXPECT_NE(std::string::npos, room.find("player=8"));

    const std::string error = FormatLobbyServerMessage(
        ServerMessage{ErrorResponse{3U, LobbyError::RoomFull}});
    EXPECT_NE(std::string::npos, error.find("RoomFull"));
}
