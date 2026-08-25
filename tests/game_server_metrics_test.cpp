#include <dxa/game_server/ServerMatchMetrics.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>

namespace
{
using namespace std::chrono_literals;
}

TEST(ServerMatchMetrics, SummarizesNearestRankWithoutDroppingRawSamples)
{
    dxa::game_server::ServerMatchMetrics metrics{128U};
    metrics.RecordTick(1ms);
    metrics.RecordTick(3ms);
    metrics.RecordReplication(2ms, 900U, 1U, true, 124U, 60U);

    const auto snapshot = metrics.Snapshot();
    ASSERT_EQ(2U, snapshot.tickSamples.size());
    ASSERT_EQ(1U, snapshot.replicationSamples.size());
    EXPECT_EQ(1ms, snapshot.tickSamples[0]);
    EXPECT_EQ(3ms, snapshot.tickSamples[1]);
    EXPECT_EQ(3ms, snapshot.tickP95);
    EXPECT_EQ(2ms, snapshot.replicationP95);
    EXPECT_EQ(900U, snapshot.payloadBytes);
    EXPECT_TRUE(snapshot.replicationSamples.front().keyframe);
}

TEST(ServerMatchMetrics, RejectsSampleCapacityOverflow)
{
    dxa::game_server::ServerMatchMetrics tickMetrics{2U};
    tickMetrics.RecordTick(1ms);
    tickMetrics.RecordTick(2ms);
    EXPECT_THROW(tickMetrics.RecordTick(3ms), std::overflow_error);

    dxa::game_server::ServerMatchMetrics replicationMetrics{1U};
    replicationMetrics.RecordReplication(
        1ms,
        100U,
        1U,
        true,
        2U,
        0U);
    EXPECT_THROW(
        replicationMetrics.RecordReplication(
            1ms,
            100U,
            1U,
            true,
            2U,
            0U),
        std::overflow_error);
}
