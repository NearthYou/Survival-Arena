#include <dxa/engine/GraphicsDevice.hpp>
#include <dxa/engine/HybridDeferredRenderer.hpp>
#include <dxa/engine/InputState.hpp>
#include <dxa/engine/Window.hpp>
#include <dxa/engine/benchmark/StressScene.hpp>

#include <gtest/gtest.h>

#include <d3d11sdklayers.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
[[nodiscard]] std::vector<std::string> CollectDebugErrors(ID3D11InfoQueue* const infoQueue)
{
    std::vector<std::string> errors;
    if (infoQueue == nullptr)
    {
        return errors;
    }

    const std::uint64_t messageCount = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
    for (std::uint64_t index = 0; index < messageCount; ++index)
    {
        SIZE_T messageSize = 0;
        if (FAILED(infoQueue->GetMessage(index, nullptr, &messageSize)))
        {
            errors.emplace_back("failed to read DX11 debug message size");
            continue;
        }
        auto storage = std::make_unique<std::byte[]>(messageSize);
        auto* const message = reinterpret_cast<D3D11_MESSAGE*>(storage.get());
        if (FAILED(infoQueue->GetMessage(index, message, &messageSize)))
        {
            errors.emplace_back("failed to read DX11 debug message");
            continue;
        }
        if (message->Severity == D3D11_MESSAGE_SEVERITY_CORRUPTION
            || message->Severity == D3D11_MESSAGE_SEVERITY_ERROR)
        {
            errors.emplace_back(message->pDescription, message->DescriptionByteLength);
        }
    }
    return errors;
}

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
        true});

    Microsoft::WRL::ComPtr<ID3D11InfoQueue> infoQueue;
    if (graphics.DebugLayerEnabled())
    {
        ASSERT_TRUE(SUCCEEDED(graphics.Device()->QueryInterface(
            IID_PPV_ARGS(infoQueue.ReleaseAndGetAddressOf()))));
        infoQueue->ClearStoredMessages();
    }

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

    renderer.SetControlledPlayerPosition({20.0F, 0.0F, 10.0F});
    graphics.BeginFrame(ClearColor);
    const dxa::engine::RenderStatistics movedStatistics = renderer.Render(
        graphics.Context(),
        graphics.BackBufferRenderTargetView(),
        dxa::engine::AssetSceneFrame{2, 1.0 / 60.0, 96.0F / 54.0F});
    const bool movedFrameContainsRenderedPixel =
        graphics.BackBufferContainsNonClearPixel(ClearColor);
    graphics.Context()->Flush();
    const std::vector<std::string> debugErrors = CollectDebugErrors(infoQueue.Get());
    graphics.EndFrame(false);

    constexpr std::uint32_t ExpectedObjects = static_cast<std::uint32_t>(
        dxa::engine::benchmark::PlayerCount
        + dxa::engine::benchmark::AiCount
        + dxa::engine::benchmark::StaticInstanceCount);
    constexpr std::uint32_t ExpectedShadowDraws = static_cast<std::uint32_t>(
        dxa::engine::benchmark::PlayerCount
        + dxa::engine::benchmark::AiCount
        + 1U);
    EXPECT_EQ(ExpectedObjects, statistics.objectCount);
    EXPECT_EQ(ExpectedObjects, statistics.visibleObjectCount + statistics.culledObjectCount);
    EXPECT_EQ(ExpectedShadowDraws, statistics.shadowDrawCalls);
    EXPECT_GT(statistics.culledObjectCount, 0U);
    EXPECT_GT(statistics.gBufferDrawCalls, 0U);
    EXPECT_LT(statistics.gBufferDrawCalls, 2240U);
    EXPECT_EQ(1U, statistics.lightingDrawCalls);
    EXPECT_EQ(1U, statistics.transparentDrawCalls);
    EXPECT_EQ(statistics.objectCount, movedStatistics.objectCount);
    EXPECT_EQ(statistics.shadowDrawCalls, movedStatistics.shadowDrawCalls);
    EXPECT_EQ(statistics.gBufferDrawCalls, movedStatistics.gBufferDrawCalls);
    EXPECT_EQ(statistics.lightingDrawCalls, movedStatistics.lightingDrawCalls);
    EXPECT_EQ(statistics.transparentDrawCalls, movedStatistics.transparentDrawCalls);
    EXPECT_TRUE(renderer.ShadowMapReady());
    EXPECT_TRUE(containsRenderedPixel);
    EXPECT_TRUE(movedFrameContainsRenderedPixel);
    EXPECT_TRUE(debugErrors.empty())
        << (debugErrors.empty() ? std::string{} : debugErrors.front());

    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(
        renderer.SetControlledPlayerPosition({notANumber, 0.0F, 0.0F}),
        std::invalid_argument);
}

TEST(HybridDeferredRenderer, AppliesGenericCharacterStatesAndZoneOverrideOnWarp)
{
    dxa::engine::HybridDeferredRenderer uninitialized;
    std::vector<dxa::engine::SceneCharacterState> uninitializedPlayers(
        dxa::engine::benchmark::PlayerCount);
    EXPECT_THROW(
        uninitialized.SetPlayerStates(uninitializedPlayers),
        std::logic_error);
    EXPECT_THROW(uninitialized.SetZoneRadius(1.0F), std::logic_error);

    dxa::engine::InputState input;
    dxa::engine::Window window;
    window.Create(
        dxa::engine::WindowConfig{L"DXA match state test", 96, 54, true},
        input);
    dxa::engine::GraphicsDevice graphics;
    graphics.Initialize(dxa::engine::GraphicsDeviceConfig{
        window.NativeHandle(),
        96,
        54,
        dxa::engine::GraphicsDriver::Warp,
        true});

    Microsoft::WRL::ComPtr<ID3D11InfoQueue> infoQueue;
    if (graphics.DebugLayerEnabled())
    {
        ASSERT_TRUE(SUCCEEDED(graphics.Device()->QueryInterface(
            IID_PPV_ARGS(infoQueue.ReleaseAndGetAddressOf()))));
        infoQueue->ClearStoredMessages();
    }

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
    const dxa::engine::RenderStatistics baseline = renderer.Render(
        graphics.Context(),
        graphics.BackBufferRenderTargetView(),
        dxa::engine::AssetSceneFrame{1, 0.0, 96.0F / 54.0F});
    graphics.EndFrame(false);

    const dxa::engine::benchmark::StressScene scene =
        dxa::engine::benchmark::GenerateStressScene(20260823U);
    std::vector<dxa::engine::SceneCharacterState> players;
    std::vector<dxa::engine::SceneCharacterState> ai;
    players.reserve(scene.players.size());
    ai.reserve(scene.ai.size());
    for (const auto& instance : scene.players)
    {
        players.push_back({instance.position, true});
    }
    for (const auto& instance : scene.ai)
    {
        ai.push_back({instance.position, true});
    }
    players[0].position = {20.0F, 0.0F, 10.0F};
    players[1].active = false;
    ai[2].active = false;

    renderer.SetPlayerStates(players);
    renderer.SetAiStates(ai);
    renderer.SetZoneRadius(0.0F);
    renderer.SetZoneRadius(64.0F);

    graphics.BeginFrame(ClearColor);
    const dxa::engine::RenderStatistics updated = renderer.Render(
        graphics.Context(),
        graphics.BackBufferRenderTargetView(),
        dxa::engine::AssetSceneFrame{2, 1.0 / 60.0, 96.0F / 54.0F});
    const bool containsRenderedPixel = graphics.BackBufferContainsNonClearPixel(ClearColor);
    graphics.Context()->Flush();
    const std::vector<std::string> debugErrors = CollectDebugErrors(infoQueue.Get());
    graphics.EndFrame(false);

    EXPECT_EQ(baseline.objectCount - 2U, updated.objectCount);
    EXPECT_LT(updated.shadowDrawCalls, baseline.shadowDrawCalls);
    EXPECT_EQ(baseline.lightingDrawCalls, updated.lightingDrawCalls);
    EXPECT_EQ(baseline.transparentDrawCalls, updated.transparentDrawCalls);
    EXPECT_TRUE(containsRenderedPixel);
    EXPECT_TRUE(debugErrors.empty())
        << (debugErrors.empty() ? std::string{} : debugErrors.front());

    players.pop_back();
    EXPECT_THROW(renderer.SetPlayerStates(players), std::invalid_argument);
    players.push_back({});
    players[0].position.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(renderer.SetPlayerStates(players), std::invalid_argument);
    ai.pop_back();
    EXPECT_THROW(renderer.SetAiStates(ai), std::invalid_argument);
    EXPECT_THROW(renderer.SetZoneRadius(-1.0F), std::invalid_argument);
    EXPECT_THROW(
        renderer.SetZoneRadius(std::numeric_limits<float>::quiet_NaN()),
        std::invalid_argument);
}
} // namespace
