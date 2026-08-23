#include <dxa/engine/GraphicsDevice.hpp>
#include <dxa/engine/HybridDeferredRenderer.hpp>
#include <dxa/engine/InputState.hpp>
#include <dxa/engine/Window.hpp>
#include <dxa/engine/benchmark/StressScene.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>

namespace
{
TEST(HybridDeferredRenderer, RendersGBufferAndLightingOnWarp)
{
    dxa::engine::InputState input;
    dxa::engine::Window window;
    window.Create(
        dxa::engine::WindowConfig{L"DXA hybrid deferred test", 96, 54, true},
        input);

    dxa::engine::GraphicsDevice graphics;
    graphics.Initialize(dxa::engine::GraphicsDeviceConfig{
        window.NativeHandle(),
        96,
        54,
        dxa::engine::GraphicsDriver::Warp,
        false});

    dxa::engine::HybridDeferredRenderer renderer;
    renderer.Initialize(
        graphics.Device(),
        dxa::engine::HybridDeferredConfig{
            96,
            54,
            128,
            20260823U,
            std::filesystem::path{DXA_TEST_SHADER_ROOT},
            std::filesystem::path{DXA_TEST_ASSET_ROOT}});

    constexpr std::array ClearColor{0.025F, 0.035F, 0.060F, 1.0F};
    graphics.BeginFrame(ClearColor);
    const dxa::engine::RenderStatistics statistics = renderer.Render(
        graphics.Context(),
        graphics.BackBufferRenderTargetView(),
        dxa::engine::AssetSceneFrame{1, 0.0, 96.0F / 54.0F});
    const bool containsRenderedPixel = graphics.BackBufferContainsNonClearPixel(ClearColor);
    graphics.EndFrame(false);

    constexpr std::uint32_t ExpectedObjects = static_cast<std::uint32_t>(
        dxa::engine::benchmark::PlayerCount
        + dxa::engine::benchmark::AiCount
        + dxa::engine::benchmark::StaticInstanceCount);
    EXPECT_EQ(ExpectedObjects, statistics.objectCount);
    EXPECT_GT(statistics.gBufferDrawCalls, 0U);
    EXPECT_EQ(1U, statistics.lightingDrawCalls);
    EXPECT_TRUE(containsRenderedPixel);
}
} // namespace
