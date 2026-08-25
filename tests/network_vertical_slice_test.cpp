#include "support/game_network_fixture.hpp"

#include <dxa/bot_client/BotCoordinator.hpp>
#include <dxa/client/NetworkClientController.hpp>
#include <dxa/engine/EngineApp.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <iostream>

namespace
{
using namespace std::chrono_literals;
}

TEST(NetworkVerticalSlice, WarpClientAndHeadlessBotShareOneMatch)
{
    const auto startedAt = std::chrono::steady_clock::now();
    dxa::test::ScopedOutputCapture capture;
    dxa::protocol::RoomId room;
    dxa::protocol::MatchId match;
    std::uint32_t finishedTick = 0U;
    std::uint64_t hostSnapshots = 0U;
    std::uint64_t botSnapshots = 0U;
    {
        dxa::test::NetworkVerticalFixture fixture{
            dxa::test::ShortNetworkVerticalMatchConfig()};
        fixture.StartServers();
        dxa::client::NetworkClientController host{fixture.HostOptions(2U)};
        host.Start();
        fixture.WaitForRoom(host);
        dxa::bot_client::BotCoordinator bot{
            fixture.BotIo(),
            fixture.PlayBotOptions(*host.Room())};
        bot.Start();

        dxa::test::VerticalWatchdog watchdog{15s};
        EXPECT_EQ(0, dxa::engine::EngineApp{}.Run(
            fixture.HiddenWarpHybridOptions(300U),
            fixture.ShaderPath(),
            fixture.AssetRoot(),
            &host));
        watchdog.Finish();
        EXPECT_FALSE(watchdog.TimedOut());
        fixture.WaitForResults(host, bot);

        ASSERT_TRUE(host.Result().has_value());
        ASSERT_TRUE(bot.Result().has_value());
        EXPECT_EQ(*bot.Result(), *host.Result());
        EXPECT_EQ(60U, host.Result()->finishedTick);
        EXPECT_GE(host.SnapshotCount(), 2U);
        EXPECT_GE(bot.SnapshotCount(), 2U);
        EXPECT_EQ(2U, fixture.TokenFillCount());

        room = *host.Room();
        match = host.Result()->match;
        finishedTick = host.Result()->finishedTick;
        hostSnapshots = host.SnapshotCount();
        botSnapshots = bot.SnapshotCount();

        bot.Stop();
        host.Stop();
        fixture.StopServers();
    }
    capture.Finish();
    EXPECT_EQ(0U, dxa::test::NetworkSecretLeakCount(capture.Text(), 2U));

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt);
    std::cout << "network vertical evidence: room=" << room.value
              << " match=" << match.value
              << " host_snapshots=" << hostSnapshots
              << " bot_snapshots=" << botSnapshots
              << " finished_tick=" << finishedTick
              << " elapsed_ms=" << elapsed.count() << '\n';
}
