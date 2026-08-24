#include <dxa/lobby/LobbyServerOptions.hpp>

#include <gtest/gtest.h>

#include <initializer_list>
#include <string_view>
#include <vector>

namespace
{
[[nodiscard]] dxa::lobby::LobbyServerOptionsParseResult Parse(
    const std::initializer_list<std::string_view> arguments)
{
    const std::vector<std::string_view> values{arguments};
    return dxa::lobby::ParseLobbyServerOptions(values);
}
} // namespace

TEST(LobbyServerOptions, DefaultsToLoopbackAndRejectsPartialWorkerEndpoint)
{
    const auto defaults = Parse({});
    ASSERT_TRUE(defaults.options.has_value());
    EXPECT_EQ("127.0.0.1", defaults.options->bindAddress);
    EXPECT_EQ(7000U, defaults.options->port);
    EXPECT_FALSE(defaults.options->worker.has_value());

    const auto partial = Parse({"--worker-host", "127.0.0.1"});
    EXPECT_FALSE(partial.options.has_value());
    EXPECT_FALSE(partial.error.empty());
}

TEST(LobbyServerOptions, ParsesCompleteWorkerAndCustomListener)
{
    const auto parsed = Parse({
        "--bind", "0.0.0.0",
        "--port", "7200",
        "--worker-host", "game.internal",
        "--worker-tcp-port", "7300",
        "--worker-udp-port", "7301"});

    ASSERT_TRUE(parsed.options.has_value());
    EXPECT_EQ("0.0.0.0", parsed.options->bindAddress);
    EXPECT_EQ(7200U, parsed.options->port);
    ASSERT_TRUE(parsed.options->worker.has_value());
    EXPECT_EQ("game.internal", parsed.options->worker->host);
    EXPECT_EQ(7300U, parsed.options->worker->tcpPort);
    EXPECT_EQ(7301U, parsed.options->worker->udpPort);
}

TEST(LobbyServerOptions, RejectsInvalidPortsAddressesDuplicatesAndUnknownOptions)
{
    EXPECT_FALSE(Parse({"--port", "0"}).options.has_value());
    EXPECT_FALSE(Parse({"--port", "65536"}).options.has_value());
    EXPECT_FALSE(Parse({"--port", "seven"}).options.has_value());
    EXPECT_FALSE(Parse({"--bind", "not-an-address"}).options.has_value());
    EXPECT_FALSE(Parse({"--port", "7000", "--port", "7001"}).options.has_value());
    EXPECT_FALSE(Parse({"--unknown", "value"}).options.has_value());
}
