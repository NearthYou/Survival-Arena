#include <dxa/engine/RenderPass.hpp>
#include <dxa/engine/benchmark/GpuPassTiming.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <stdexcept>

namespace
{
using dxa::engine::RenderPass;
using dxa::engine::benchmark::CalculatePassDurations;
using dxa::engine::benchmark::TimestampSequence;

TEST(GpuPassTiming, CalculatesHybridPassDurationsFromOrderedMarkers)
{
    const TimestampSequence sequence{
        1000,
        100,
        std::array<std::uint64_t, 4>{120, 150, 190, 200},
        200,
        4};
    constexpr std::array passes{
        RenderPass::Shadow,
        RenderPass::GBuffer,
        RenderPass::DeferredLighting,
        RenderPass::Transparent};

    const auto durations = CalculatePassDurations(sequence, passes);

    ASSERT_TRUE(durations.totalMilliseconds.has_value());
    ASSERT_TRUE(durations.shadowMilliseconds.has_value());
    ASSERT_TRUE(durations.gBufferMilliseconds.has_value());
    ASSERT_TRUE(durations.lightingMilliseconds.has_value());
    ASSERT_TRUE(durations.transparentMilliseconds.has_value());
    EXPECT_DOUBLE_EQ(100.0, *durations.totalMilliseconds);
    EXPECT_DOUBLE_EQ(20.0, *durations.shadowMilliseconds);
    EXPECT_DOUBLE_EQ(30.0, *durations.gBufferMilliseconds);
    EXPECT_DOUBLE_EQ(40.0, *durations.lightingMilliseconds);
    EXPECT_DOUBLE_EQ(10.0, *durations.transparentMilliseconds);
    EXPECT_FALSE(durations.forwardMilliseconds.has_value());
}

TEST(GpuPassTiming, MapsAnUnmarkedFrameToForwardDuration)
{
    const TimestampSequence sequence{1000, 10, {}, 15, 0};

    const auto durations = CalculatePassDurations(sequence, {});

    ASSERT_TRUE(durations.totalMilliseconds.has_value());
    ASSERT_TRUE(durations.forwardMilliseconds.has_value());
    EXPECT_DOUBLE_EQ(5.0, *durations.totalMilliseconds);
    EXPECT_DOUBLE_EQ(5.0, *durations.forwardMilliseconds);
}

TEST(GpuPassTiming, RejectsWrongPassOrderAndNonMonotonicTimestamps)
{
    const TimestampSequence sequence{
        1000,
        100,
        std::array<std::uint64_t, 4>{120, 150, 190, 200},
        200,
        4};
    constexpr std::array wrongOrder{
        RenderPass::GBuffer,
        RenderPass::Shadow,
        RenderPass::DeferredLighting,
        RenderPass::Transparent};
    TimestampSequence nonMonotonic = sequence;
    nonMonotonic.markers[2] = 140;

    EXPECT_THROW((void)CalculatePassDurations(sequence, wrongOrder), std::invalid_argument);
    EXPECT_THROW(
        (void)CalculatePassDurations(
            nonMonotonic,
            std::array{
                RenderPass::Shadow,
                RenderPass::GBuffer,
                RenderPass::DeferredLighting,
                RenderPass::Transparent}),
        std::invalid_argument);
}

TEST(GpuPassTiming, RejectsZeroFrequencyAndMarkerCountMismatch)
{
    TimestampSequence zeroFrequency{1000, 100, {}, 200, 0};
    zeroFrequency.frequency = 0;
    const TimestampSequence mismatch{
        1000,
        100,
        std::array<std::uint64_t, 4>{120, 0, 0, 0},
        200,
        1};

    EXPECT_THROW((void)CalculatePassDurations(zeroFrequency, {}), std::invalid_argument);
    EXPECT_THROW((void)CalculatePassDurations(mismatch, {}), std::invalid_argument);
}
} // namespace
