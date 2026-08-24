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

TEST(LobbyServerOptions, DefaultsToSeparateLoopbackListeners)
{
    const auto defaults = Parse({});
    ASSERT_TRUE(defaults.options.has_value());
    EXPECT_EQ("127.0.0.1", defaults.options->bindAddress);
    EXPECT_EQ(7000U, defaults.options->port);
    EXPECT_EQ("127.0.0.1", defaults.options->workerBindAddress);
    EXPECT_EQ(7001U, defaults.options->workerPort);
    EXPECT_FALSE(Parse({
        "--worker-host", "127.0.0.1"}).options.has_value());
    EXPECT_FALSE(Parse({
        "--worker-tcp-port", "7100"}).options.has_value());
    EXPECT_FALSE(Parse({
        "--worker-udp-port", "7101"}).options.has_value());
}

TEST(LobbyServerOptions, ParsesSeparateWorkerControlListener)
{
    const auto parsed = Parse({
        "--bind", "0.0.0.0",
        "--port", "7200",
        "--worker-bind", "127.0.0.1",
        "--worker-port", "7201"});

    ASSERT_TRUE(parsed.options.has_value());
    EXPECT_EQ("0.0.0.0", parsed.options->bindAddress);
    EXPECT_EQ(7200U, parsed.options->port);
    EXPECT_EQ("127.0.0.1", parsed.options->workerBindAddress);
    EXPECT_EQ(7201U, parsed.options->workerPort);
}

TEST(LobbyServerOptions, RejectsInvalidPortsAddressesDuplicatesAndUnknownOptions)
{
    EXPECT_FALSE(Parse({"--port", "0"}).options.has_value());
    EXPECT_FALSE(Parse({"--port", "65536"}).options.has_value());
    EXPECT_FALSE(Parse({"--port", "seven"}).options.has_value());
    EXPECT_FALSE(Parse({"--bind", "not-an-address"}).options.has_value());
    EXPECT_FALSE(Parse({"--port", "7000", "--port", "7001"}).options.has_value());
    EXPECT_FALSE(Parse({"--worker-port", "0"}).options.has_value());
    EXPECT_FALSE(Parse({"--worker-port", "65536"}).options.has_value());
    EXPECT_FALSE(Parse({"--worker-bind", "not-an-address"}).options.has_value());
    EXPECT_FALSE(Parse({
        "--worker-port", "7001",
        "--worker-port", "7002"}).options.has_value());
    EXPECT_FALSE(Parse({"--unknown", "value"}).options.has_value());
}
