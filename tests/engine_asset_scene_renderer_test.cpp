#include <dxa/engine/AssetSceneRenderer.hpp>
#include <dxa/engine/GraphicsDevice.hpp>
#include <dxa/engine/InputState.hpp>
#include <dxa/engine/Window.hpp>
#include <dxa/engine/benchmark/StressScene.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>

namespace
{
TEST(AssetSceneRenderer, RendersLockedForwardBaselinePopulationOnWarp)
{
    dxa::engine::InputState input;
    dxa::engine::Window window;
    window.Create(
        dxa::engine::WindowConfig{L"DXA stress scene test", 96, 54, true},
        input);

    dxa::engine::GraphicsDevice graphics;
    graphics.Initialize(dxa::engine::GraphicsDeviceConfig{
        window.NativeHandle(),
        96,
        54,
        dxa::engine::GraphicsDriver::Warp,
        false});

    dxa::engine::AssetSceneRenderer renderer;
    renderer.Initialize(
        graphics.Device(),
        std::filesystem::path{DXA_TEST_ASSET_SHADER_PATH},
        std::filesystem::path{DXA_TEST_ASSET_ROOT},
        dxa::engine::AssetSceneConfig{20260823U});

    constexpr std::array ClearColor{0.025F, 0.035F, 0.060F, 1.0F};
    graphics.BeginFrame(ClearColor);
    const auto statistics = renderer.Render(
        graphics.Context(),
        dxa::engine::AssetSceneFrame{1, 0.0, 96.0F / 54.0F});
    const bool containsRenderedPixel = graphics.BackBufferContainsNonClearPixel(ClearColor);
    graphics.EndFrame(false);

    constexpr std::uint32_t ExpectedObjects = static_cast<std::uint32_t>(
        dxa::engine::benchmark::PlayerCount
        + dxa::engine::benchmark::AiCount
        + dxa::engine::benchmark::StaticInstanceCount);
    EXPECT_EQ(ExpectedObjects, statistics.objectCount);
    EXPECT_GE(statistics.drawCalls, statistics.objectCount);
    EXPECT_GT(statistics.triangleCount, statistics.drawCalls);
    EXPECT_TRUE(renderer.AssetSceneReady());
    EXPECT_TRUE(containsRenderedPixel);
}
} // namespace
