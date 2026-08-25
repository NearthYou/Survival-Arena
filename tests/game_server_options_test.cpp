#include <dxa/game_server/GameServerOptions.hpp>

#include <gtest/gtest.h>

#include <array>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
[[nodiscard]] dxa::game_server::GameServerOptionsParseResult Parse(
    const std::initializer_list<std::string_view> arguments)
{
    const std::vector<std::string_view> values{arguments};
    return dxa::game_server::ParseGameServerOptions(values);
}
} // namespace

TEST(GameServerOptions, DefaultsToLoopbackWorkerAndGamePorts)
{
    const auto parsed = Parse({});

    ASSERT_TRUE(parsed.options.has_value());
    EXPECT_EQ("127.0.0.1", parsed.options->lobbyControlHost);
    EXPECT_EQ(7001U, parsed.options->lobbyControlPort);
    EXPECT_EQ(dxa::protocol::WorkerId{1U}, parsed.options->worker);
    EXPECT_EQ("127.0.0.1", parsed.options->advertisedHost);
    EXPECT_EQ("127.0.0.1", parsed.options->gameBindAddress);
    EXPECT_EQ(7100U, parsed.options->gameTcpPort);
    EXPECT_EQ(7101U, parsed.options->gameUdpPort);
}

TEST(GameServerOptions, ParsesLoopbackWorkerAndGamePorts)
{
    const auto parsed = Parse({
        "--lobby-control-host", "127.0.0.2",
        "--lobby-control-port", "7201",
        "--worker-id", "9",
        "--advertise-host", "game.example.test",
        "--game-bind", "0.0.0.0",
        "--game-tcp-port", "7300",
        "--game-udp-port", "7301"});

    ASSERT_TRUE(parsed.options.has_value());
    EXPECT_EQ("127.0.0.2", parsed.options->lobbyControlHost);
    EXPECT_EQ(7201U, parsed.options->lobbyControlPort);
    EXPECT_EQ(dxa::protocol::WorkerId{9U}, parsed.options->worker);
    EXPECT_EQ("game.example.test", parsed.options->advertisedHost);
    EXPECT_EQ("0.0.0.0", parsed.options->gameBindAddress);
    EXPECT_EQ(7300U, parsed.options->gameTcpPort);
    EXPECT_EQ(7301U, parsed.options->gameUdpPort);
}

TEST(GameServerOptions, ParsesFullStateMetricsOutput)
{
    const auto parsed = Parse({
        "--replication-mode", "full-state",
        "--metrics-output-root", "out/network-load"});

    ASSERT_TRUE(parsed.options.has_value()) << parsed.error;
    EXPECT_EQ(
        dxa::protocol::ReplicationMode::FullState,
        parsed.options->replicationMode);
    EXPECT_EQ("out/network-load", parsed.options->metricsOutputRoot);
}

TEST(GameServerOptions, ParsesEveryReplicationMode)
{
    const std::array cases{
        std::pair{
            std::string_view{"full-state"},
            dxa::protocol::ReplicationMode::FullState},
        std::pair{
            std::string_view{"interest-full"},
            dxa::protocol::ReplicationMode::InterestFullPrecision},
        std::pair{
            std::string_view{"interest-quantized"},
            dxa::protocol::ReplicationMode::InterestQuantized},
        std::pair{
            std::string_view{"interest-delta"},
            dxa::protocol::ReplicationMode::InterestDelta}};

    for (const auto& [name, mode] : cases)
    {
        const auto parsed = Parse({"--replication-mode", name});
        ASSERT_TRUE(parsed.options.has_value()) << parsed.error;
        EXPECT_EQ(mode, parsed.options->replicationMode);
    }
}

TEST(GameServerOptions, RejectsInvalidBoundariesDuplicatesAndUnknownOptions)
{
    EXPECT_FALSE(Parse({"--lobby-control-port", "0"}).options.has_value());
    EXPECT_FALSE(Parse({"--lobby-control-port", "65536"}).options.has_value());
    EXPECT_FALSE(Parse({"--game-tcp-port", "0"}).options.has_value());
    EXPECT_FALSE(Parse({"--game-udp-port", "65536"}).options.has_value());
    EXPECT_FALSE(Parse({"--worker-id", "0"}).options.has_value());
    EXPECT_FALSE(Parse({"--worker-id", "worker"}).options.has_value());
    EXPECT_FALSE(Parse({
        "--lobby-control-host", "not-an-address"}).options.has_value());
    EXPECT_FALSE(Parse({"--game-bind", "not-an-address"}).options.has_value());
    EXPECT_FALSE(Parse({"--advertise-host", "bad\nhost"}).options.has_value());
    EXPECT_FALSE(Parse({"--advertise-host", ""}).options.has_value());
    const std::string longHost(256U, 'a');
    EXPECT_FALSE(Parse({"--advertise-host", longHost}).options.has_value());
    EXPECT_FALSE(Parse({
        "--game-tcp-port", "7100",
        "--game-tcp-port", "7101"}).options.has_value());
    EXPECT_FALSE(Parse({
        "--replication-mode", "unknown"}).options.has_value());
    EXPECT_FALSE(Parse({"--metrics-output-root", ""}).options.has_value());
    EXPECT_FALSE(Parse({
        "--metrics-output-root", "first",
        "--metrics-output-root", "second"}).options.has_value());
    EXPECT_FALSE(Parse({"--unknown", "value"}).options.has_value());
}
