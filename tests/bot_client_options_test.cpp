#include <dxa/bot_client/BotClientOptions.hpp>

#include <gtest/gtest.h>

#include <initializer_list>
#include <string_view>
#include <vector>

namespace
{
[[nodiscard]] dxa::bot_client::BotClientOptionsParseResult Parse(
    const std::initializer_list<std::string_view> arguments)
{
    const std::vector<std::string_view> values{arguments};
    return dxa::bot_client::ParseBotClientOptions(values);
}
} // namespace

TEST(BotClientOptions, AcceptsOneToTwentyThreeBots)
{
    const auto one = Parse({"--room", "7", "--count", "1"});
    ASSERT_TRUE(one.options.has_value());
    EXPECT_EQ(1U, one.options->count);
    EXPECT_EQ("127.0.0.1", one.options->host);
    EXPECT_EQ(7000U, one.options->port);

    const auto twentyThree = Parse({"--room", "7", "--count", "23"});
    ASSERT_TRUE(twentyThree.options.has_value());
    EXPECT_EQ(23U, twentyThree.options->count);

    EXPECT_FALSE(Parse({"--room", "7", "--count", "0"}).options.has_value());
    EXPECT_FALSE(Parse({"--room", "7", "--count", "24"}).options.has_value());
}

TEST(BotClientOptions, ParsesCustomHostPortAndRequiresValidRoom)
{
    const auto custom = Parse({
        "--host", "lobby.internal",
        "--port", "7200",
        "--room", "42"});
    ASSERT_TRUE(custom.options.has_value());
    EXPECT_EQ("lobby.internal", custom.options->host);
    EXPECT_EQ(7200U, custom.options->port);
    EXPECT_EQ(dxa::protocol::RoomId{42U}, custom.options->room);
    EXPECT_EQ(1U, custom.options->count);

    EXPECT_FALSE(Parse({}).options.has_value());
    EXPECT_FALSE(Parse({"--room", "0"}).options.has_value());
    EXPECT_FALSE(Parse({"--room", "4294967296"}).options.has_value());
    EXPECT_FALSE(Parse({"--room", "seven"}).options.has_value());
}

TEST(BotClientOptions, RejectsInvalidPortsMissingValuesDuplicatesAndUnknownOptions)
{
    EXPECT_FALSE(Parse({"--room", "7", "--port", "0"}).options.has_value());
    EXPECT_FALSE(Parse({"--room", "7", "--port", "65536"}).options.has_value());
    EXPECT_FALSE(Parse({"--room", "7", "--port", "seven"}).options.has_value());
    EXPECT_FALSE(Parse({"--room"}).options.has_value());
    EXPECT_FALSE(Parse({"--room", "7", "--room", "8"}).options.has_value());
    EXPECT_FALSE(Parse({"--room", "7", "--unknown", "1"}).options.has_value());
}

TEST(BotClientOptions, PlayModeAcceptsOneToTwentyThreeBots)
{
    const auto one = Parse({"--room", "7", "--count", "1", "--play"});
    ASSERT_TRUE(one.options.has_value());
    EXPECT_TRUE(one.options->play);

    const auto twentyThree = Parse({
        "--room", "7", "--count", "23", "--play"});
    ASSERT_TRUE(twentyThree.options.has_value());
    EXPECT_EQ(23U, twentyThree.options->count);
    EXPECT_TRUE(twentyThree.options->play);

    EXPECT_FALSE(Parse({
        "--room", "7", "--play", "--play"}).options.has_value());
}
