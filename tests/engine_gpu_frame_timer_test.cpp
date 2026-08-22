#include <dxa/engine/GpuFrameTimer.hpp>
#include <dxa/engine/GraphicsDevice.hpp>
#include <dxa/engine/InputState.hpp>
#include <dxa/engine/SystemMetrics.hpp>
#include <dxa/engine/Window.hpp>

#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
TEST(GpuFrameTimer, ResolvesEverySubmittedWarpTimestampWithoutAFrameLoopStall)
{
    dxa::engine::InputState input;
    dxa::engine::Window window;
    window.Create(dxa::engine::WindowConfig{L"DXA GPU timer test", 64, 64, true}, input);

    dxa::engine::GraphicsDevice graphics;
    graphics.Initialize(dxa::engine::GraphicsDeviceConfig{
        window.NativeHandle(),
        64,
        64,
        dxa::engine::GraphicsDriver::Warp,
        false});

    dxa::engine::GpuFrameTimer timer;
    timer.Initialize(graphics.Device());
    std::vector<dxa::engine::GpuFrameResult> resolved;
    constexpr std::array ClearColor{0.0F, 0.0F, 0.0F, 1.0F};
    for (std::uint64_t frameIndex = 1; frameIndex <= 4; ++frameIndex)
    {
        graphics.BeginFrame(ClearColor);
        ASSERT_TRUE(timer.BeginFrame(graphics.Context(), frameIndex));
        timer.EndFrame(graphics.Context());
        graphics.EndFrame(false);

        auto ready = timer.ResolveReady(graphics.Context());
        resolved.insert(resolved.end(), ready.begin(), ready.end());
    }

    auto drained = timer.Drain(graphics.Context(), std::chrono::seconds{2});
    resolved.insert(resolved.end(), drained.begin(), drained.end());

    ASSERT_EQ(4U, resolved.size());
    for (std::uint64_t frameIndex = 1; frameIndex <= 4; ++frameIndex)
    {
        const auto found = std::ranges::find_if(
            resolved,
            [frameIndex](const dxa::engine::GpuFrameResult& result) {
                return result.frameIndex == frameIndex;
            });
        ASSERT_NE(resolved.end(), found);
        if (found->elapsedMilliseconds.has_value())
        {
            EXPECT_GE(*found->elapsedMilliseconds, 0.0);
        }
    }
}

TEST(SystemMetrics, ReportsCurrentWorkingSetAndAdapterName)
{
    dxa::engine::InputState input;
    dxa::engine::Window window;
    window.Create(dxa::engine::WindowConfig{L"DXA metrics test", 64, 64, true}, input);

    dxa::engine::GraphicsDevice graphics;
    graphics.Initialize(dxa::engine::GraphicsDeviceConfig{
        window.NativeHandle(),
        64,
        64,
        dxa::engine::GraphicsDriver::Warp,
        false});

    EXPECT_GT(dxa::engine::GetCurrentProcessWorkingSetBytes(), 0U);
    EXPECT_FALSE(dxa::engine::GetAdapterNameUtf8(graphics.Device()).empty());
}

TEST(GpuFrameTimer, AllocatesOnePendingSlotForEveryConfiguredMeasuredFrame)
{
    dxa::engine::InputState input;
    dxa::engine::Window window;
    window.Create(dxa::engine::WindowConfig{L"DXA GPU timer capacity test", 64, 64, true}, input);

    dxa::engine::GraphicsDevice graphics;
    graphics.Initialize(dxa::engine::GraphicsDeviceConfig{
        window.NativeHandle(),
        64,
        64,
        dxa::engine::GraphicsDriver::Warp,
        false});

    constexpr std::size_t ConfiguredFrames = 20;
    dxa::engine::GpuFrameTimer timer;
    timer.Initialize(graphics.Device(), ConfiguredFrames);
    constexpr std::array ClearColor{0.0F, 0.0F, 0.0F, 1.0F};
    for (std::uint64_t frameIndex = 1; frameIndex <= ConfiguredFrames; ++frameIndex)
    {
        graphics.BeginFrame(ClearColor);
        EXPECT_TRUE(timer.BeginFrame(graphics.Context(), frameIndex));
        timer.EndFrame(graphics.Context());
        graphics.EndFrame(false);
    }

    const auto results = timer.Drain(graphics.Context(), std::chrono::seconds{2});
    EXPECT_EQ(ConfiguredFrames, results.size());
}
} // namespace
