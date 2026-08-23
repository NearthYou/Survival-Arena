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
#include <memory>
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
    EXPECT_TRUE(renderer.ShadowMapReady());
    EXPECT_TRUE(containsRenderedPixel);
    EXPECT_TRUE(debugErrors.empty())
        << (debugErrors.empty() ? std::string{} : debugErrors.front());
}
} // namespace
