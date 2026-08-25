#include "support/game_network_fixture.hpp"

#include <dxa/bot_client/BotCoordinator.hpp>
#include <dxa/client/NetworkClientController.hpp>
#include <dxa/engine/EngineApp.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace
{
using namespace std::chrono_literals;
}

TEST(NetworkLoadVertical, WarpHostAndTwentyThreeBotSessionsFinishOneMatch)
{
    const auto startedAt = std::chrono::steady_clock::now();
    dxa::test::ScopedOutputCapture capture;
    dxa::protocol::RoomId room;
    dxa::protocol::MatchId match;
    std::uint32_t finishedTick = 0U;
    std::uint64_t hostSnapshots = 0U;
    std::size_t botSessions = 0U;
    {
        dxa::test::NetworkVerticalFixture fixture{
            dxa::test::ShortNetworkVerticalMatchConfig()};
        fixture.StartServers();

        dxa::client::NetworkClientController host{fixture.HostOptions(24U)};
        host.Start();
        fixture.WaitForRoom(host);

        dxa::bot_client::BotCoordinator bots{
            fixture.BotIo(),
            fixture.PlayBotOptions(*host.Room(), 23U)};
        bots.Start();

        dxa::test::VerticalWatchdog watchdog{20s};
        EXPECT_EQ(0, dxa::engine::EngineApp{}.Run(
            fixture.HiddenWarpHybridOptions(420U),
            fixture.ShaderPath(),
            fixture.AssetRoot(),
            &host));
        watchdog.Finish();
        EXPECT_FALSE(watchdog.TimedOut());
        fixture.WaitForResults(host, bots, 23U);

        ASSERT_TRUE(host.Result().has_value());
        const dxa::bot_client::BotCoordinatorReport report = bots.Report();
        ASSERT_TRUE(report.result.has_value());
        ASSERT_EQ(23U, report.sessions.size());
        EXPECT_EQ(*host.Result(), *report.result);
        EXPECT_TRUE(std::all_of(
            report.sessions.begin(),
            report.sessions.end(),
            [](const dxa::bot_client::BotSessionReport& session) {
                return session.exitCode == 0
                    && session.snapshotsApplied >= 2U;
            }));
        EXPECT_EQ(60U, host.Result()->finishedTick);
        EXPECT_GE(host.SnapshotCount(), 2U);
        EXPECT_EQ(24U, fixture.TokenFillCount());

        room = *host.Room();
        match = host.Result()->match;
        finishedTick = host.Result()->finishedTick;
        hostSnapshots = host.SnapshotCount();
        botSessions = report.sessions.size();

        bots.Stop();
        host.Stop();
        fixture.StopServers();
    }
    capture.Finish();
    EXPECT_EQ(0U, dxa::test::NetworkSecretLeakCount(capture.Text(), 24U));

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt);
    std::cout << "network load vertical evidence: room=" << room.value
              << " match=" << match.value
              << " host_snapshots=" << hostSnapshots
              << " bot_sessions=" << botSessions
              << " finished_tick=" << finishedTick
              << " elapsed_ms=" << elapsed.count() << '\n';
}
