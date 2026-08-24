#include <dxa/lobby_cli/LobbyCliCommand.hpp>

#include <gtest/gtest.h>

using dxa::lobby_cli::LobbyCliCommandType;
using dxa::lobby_cli::ParseLobbyCliCommand;
using dxa::protocol::RoomId;

TEST(LobbyCliCommand, ParsesEverySupportedCommand)
{
    EXPECT_EQ(
        LobbyCliCommandType::List,
        ParseLobbyCliCommand("list").command->type);
    EXPECT_EQ(
        LobbyCliCommandType::Create,
        ParseLobbyCliCommand("create").command->type);
    EXPECT_EQ(
        RoomId{42U},
        ParseLobbyCliCommand("join 42").command->room);
    EXPECT_EQ(
        LobbyCliCommandType::Leave,
        ParseLobbyCliCommand("leave").command->type);
    EXPECT_TRUE(ParseLobbyCliCommand("ready on").command->ready);
    EXPECT_FALSE(ParseLobbyCliCommand("ready off").command->ready);
    EXPECT_EQ(
        LobbyCliCommandType::Start,
        ParseLobbyCliCommand("start").command->type);
    EXPECT_EQ(
        LobbyCliCommandType::Quit,
        ParseLobbyCliCommand("quit").command->type);
}

TEST(LobbyCliCommand, RejectsMissingMalformedAndExtraArguments)
{
    EXPECT_FALSE(ParseLobbyCliCommand("").command.has_value());
    EXPECT_FALSE(ParseLobbyCliCommand("join").command.has_value());
    EXPECT_FALSE(ParseLobbyCliCommand("join 0").command.has_value());
    EXPECT_FALSE(ParseLobbyCliCommand("join 4294967296").command.has_value());
    EXPECT_FALSE(ParseLobbyCliCommand("join seven").command.has_value());
    EXPECT_FALSE(ParseLobbyCliCommand("ready").command.has_value());
    EXPECT_FALSE(ParseLobbyCliCommand("ready maybe").command.has_value());
    EXPECT_FALSE(ParseLobbyCliCommand("start now").command.has_value());
    EXPECT_FALSE(ParseLobbyCliCommand("unknown").command.has_value());
}
