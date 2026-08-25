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

        dxa::client::NetworkClientController host{
            fixture.HostOptions(24U, true)};
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
                    && session.snapshotsApplied >= 2U
                    && session.keyframesApplied >= 1U
                    && session.receivedTcpBytes > 0U
                    && session.receivedUdpBytes > 0U;
            }));
        EXPECT_EQ(60U, host.Result()->finishedTick);
        EXPECT_TRUE(host.ShouldClose());
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

TEST(NetworkLoadVertical, EveryReplicationModeMatchesFullStateResult)
{
    const auto runMode = [](const dxa::protocol::ReplicationMode mode) {
        dxa::test::ScopedOutputCapture capture;
        dxa::protocol::GameMatchResult result;
        {
            dxa::test::NetworkVerticalFixture fixture{
                dxa::test::ShortNetworkVerticalMatchConfig(),
                mode};
            fixture.StartServers();
            dxa::client::NetworkClientController host{
                fixture.HostOptions(24U, true)};
            host.Start();
            fixture.WaitForRoom(host);

            dxa::bot_client::BotCoordinator bots{
                fixture.BotIo(),
                fixture.PlayBotOptions(*host.Room(), 23U)};
            bots.Start();
            dxa::test::VerticalWatchdog watchdog{20s};
            const int exitCode = dxa::engine::EngineApp{}.Run(
                fixture.HiddenWarpHybridOptions(420U),
                fixture.ShaderPath(),
                fixture.AssetRoot(),
                &host);
            watchdog.Finish();
            if (exitCode != 0 || watchdog.TimedOut())
            {
                throw std::runtime_error{"replication mode WARP run failed"};
            }
            fixture.WaitForResults(host, bots, 23U);
            if (!host.Result().has_value())
            {
                throw std::runtime_error{"replication mode has no result"};
            }
            const dxa::bot_client::BotCoordinatorReport report = bots.Report();
            if (!report.result.has_value()
                || report.sessions.size() != 23U
                || *report.result != *host.Result()
                || !std::all_of(
                    report.sessions.begin(),
                    report.sessions.end(),
                    [](const dxa::bot_client::BotSessionReport& session) {
                        return session.exitCode == 0
                            && session.snapshotsApplied >= 2U
                            && session.keyframesApplied >= 1U;
                    }))
            {
                throw std::runtime_error{
                    "replication mode result parity failed"};
            }
            result = *host.Result();
            bots.Stop();
            host.Stop();
            fixture.StopServers();
        }
        capture.Finish();
        EXPECT_EQ(0U, dxa::test::NetworkSecretLeakCount(capture.Text(), 24U));
        return result;
    };

    const dxa::protocol::GameMatchResult baseline = runMode(
        dxa::protocol::ReplicationMode::FullState);
    for (const dxa::protocol::ReplicationMode mode : {
             dxa::protocol::ReplicationMode::InterestFullPrecision,
             dxa::protocol::ReplicationMode::InterestQuantized,
             dxa::protocol::ReplicationMode::InterestDelta})
    {
        EXPECT_EQ(baseline, runMode(mode));
    }
}

TEST(GameServerIntegration, ImpairmentProfileFinishesWithKeyframeRecovery)
{
    const dxa::protocol::DatagramShaperConfig impairment{
        50ms,
        10ms,
        200U,
        20260825U};
    std::uint64_t serverDrops = 0U;
    std::uint64_t clientDrops = 0U;
    std::uint64_t keyframeRequests = 0U;
    std::uint64_t queueOverflows = 0U;
    bool everyBotFinished = false;
    bool everyMeasurementPositive = false;
    std::uint64_t completedServerDrops = 0U;
    dxa::test::ScopedOutputCapture capture;
    {
        dxa::test::NetworkVerticalFixture fixture{
            dxa::test::ShortNetworkVerticalMatchConfig(),
            dxa::protocol::ReplicationMode::InterestDelta,
            impairment};
        fixture.StartServers();

        dxa::client::NetworkClientController host{
            fixture.HostOptions(24U, true)};
        host.Start();
        fixture.WaitForRoom(host);

        dxa::bot_client::BotCoordinator bots{
            fixture.BotIo(),
            fixture.PlayBotOptions(*host.Room(), 23U)};
        bots.Start();

        dxa::test::VerticalWatchdog watchdog{20s};
        EXPECT_EQ(0, dxa::engine::EngineApp{}.Run(
            fixture.HiddenWarpHybridOptions(480U),
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
        EXPECT_EQ(60U, host.Result()->finishedTick);

        const auto hostMetrics = host.Metrics();
        const auto serverShaping = fixture.UdpShapingMetrics();
        const auto completedMetrics = fixture.CompletedMetrics();
        ASSERT_EQ(1U, completedMetrics.size());
        completedServerDrops = completedMetrics.front().udpDatagramsDropped;
        clientDrops = hostMetrics.udpDatagramsDropped;
        keyframeRequests = hostMetrics.keyframeRequests;
        queueOverflows = hostMetrics.shapedQueueOverflows
            + serverShaping.overflows;
        everyBotFinished = true;
        everyMeasurementPositive = hostMetrics.measurementNanoseconds > 0U;
        for (const dxa::bot_client::BotSessionReport& session
             : report.sessions)
        {
            everyBotFinished = everyBotFinished && session.exitCode == 0;
            everyMeasurementPositive = everyMeasurementPositive
                && session.measurementNanoseconds > 0U;
            clientDrops += session.udpDatagramsDropped;
            keyframeRequests += session.keyframeRequests;
            queueOverflows += session.shapedQueueOverflows;
        }
        serverDrops = serverShaping.dropped;

        bots.Stop();
        host.Stop();
        fixture.StopServers();
    }
    capture.Finish();
    EXPECT_EQ(0U, dxa::test::NetworkSecretLeakCount(capture.Text(), 24U));
    EXPECT_TRUE(everyBotFinished);
    EXPECT_TRUE(everyMeasurementPositive);
    EXPECT_EQ(16U, serverDrops);
    EXPECT_EQ(16U, completedServerDrops);
    EXPECT_GT(clientDrops, 0U);
    EXPECT_GT(keyframeRequests, 0U);
    EXPECT_EQ(0U, queueOverflows);
}
