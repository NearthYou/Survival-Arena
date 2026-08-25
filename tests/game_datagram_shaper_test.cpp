#include <dxa/protocol/DatagramShaper.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>

namespace
{
using namespace std::chrono_literals;
using namespace dxa::protocol;

[[nodiscard]] constexpr std::uint64_t Peer1() noexcept
{
    return 1U;
}

TEST(DatagramShaper, RepeatsDropAndDelayForSameSeed)
{
    const DatagramShaperConfig config{50ms, 10ms, 200U, 20260825U};
    const DatagramShaper first{config, DatagramDirection::ClientToServer};
    const DatagramShaper second{config, DatagramDirection::ClientToServer};

    for (std::uint64_t ordinal = 1U; ordinal <= 10000U; ++ordinal)
    {
        EXPECT_EQ(
            first.Decide(Peer1(), ordinal),
            second.Decide(Peer1(), ordinal));
    }
}

TEST(DatagramShaper, FixedSampleHasLockedTwoPercentLossSequence)
{
    const DatagramShaper shaper{
        {50ms, 10ms, 200U, 20260825U},
        DatagramDirection::ServerToClient};
    std::uint32_t dropped = 0U;
    for (std::uint64_t ordinal = 1U; ordinal <= 10000U; ++ordinal)
    {
        dropped += shaper.Decide(Peer1(), ordinal).drop ? 1U : 0U;
    }
    EXPECT_EQ(213U, dropped);
}

TEST(DatagramShaper, DelayStaysInsideConfiguredJitterRange)
{
    const DatagramShaper shaper{
        {50ms, 10ms, 0U, 20260825U},
        DatagramDirection::ClientToServer};
    for (std::uint64_t ordinal = 1U; ordinal <= 10000U; ++ordinal)
    {
        const ShapedDatagramDecision decision = shaper.Decide(
            Peer1(),
            ordinal);
        EXPECT_FALSE(decision.drop);
        EXPECT_GE(decision.delay, 40ms);
        EXPECT_LE(decision.delay, 60ms);
    }
}

TEST(DatagramShaper, DirectionsUseDifferentDeterministicSequences)
{
    const DatagramShaperConfig config{50ms, 10ms, 200U, 20260825U};
    const DatagramShaper clientToServer{
        config,
        DatagramDirection::ClientToServer};
    const DatagramShaper serverToClient{
        config,
        DatagramDirection::ServerToClient};
    std::uint32_t differences = 0U;
    for (std::uint64_t ordinal = 1U; ordinal <= 1000U; ++ordinal)
    {
        differences += clientToServer.Decide(Peer1(), ordinal)
                != serverToClient.Decide(Peer1(), ordinal)
            ? 1U
            : 0U;
    }
    EXPECT_GT(differences, 0U);
}

TEST(DatagramShaper, DisabledConfigHasZeroDelayAndNoDrop)
{
    const DatagramShaper shaper{{}, DatagramDirection::ClientToServer};
    EXPECT_EQ(
        ShapedDatagramDecision{},
        shaper.Decide(Peer1(), 1U));
}

TEST(DatagramShaper, RejectsInvalidConfigurationAndDirection)
{
    EXPECT_THROW(
        (void)DatagramShaper(
            {-1ms, 0ms, 0U, 20260825U},
            DatagramDirection::ClientToServer),
        std::invalid_argument);
    EXPECT_THROW(
        (void)DatagramShaper(
            {0ms, -1ms, 0U, 20260825U},
            DatagramDirection::ClientToServer),
        std::invalid_argument);
    EXPECT_THROW(
        (void)DatagramShaper(
            {0ms, 0ms, 10001U, 20260825U},
            DatagramDirection::ClientToServer),
        std::invalid_argument);
    EXPECT_THROW(
        (void)DatagramShaper(
            {50ms, 10ms, 200U, 0U},
            DatagramDirection::ClientToServer),
        std::invalid_argument);
    EXPECT_THROW(
        (void)DatagramShaper(
            {},
            static_cast<DatagramDirection>(0U)),
        std::invalid_argument);
}
} // namespace
