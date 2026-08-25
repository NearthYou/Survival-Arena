#include <dxa/engine/EngineApp.hpp>
#include <dxa/engine/RenderPath.hpp>
#include <dxa/engine/RuntimeScene.hpp>

#include <gtest/gtest.h>

#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
class TemporaryDirectory
{
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path()
            / ("dxa-engine-benchmark-" + std::to_string(suffix));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

[[nodiscard]] std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

void DrainThreadMessages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE)
    {
    }
}

[[nodiscard]] dxa::engine::EngineRunOptions HiddenHybridOptions(
    const std::uint32_t frameLimit)
{
    return dxa::engine::EngineRunOptions{
        96U,
        54U,
        frameLimit,
        true,
        false,
        true,
        dxa::engine::GraphicsDriver::Warp,
        true,
        std::nullopt,
        dxa::engine::RenderPath::HybridDeferred};
}

class ScriptedRuntimeScene final
    : public dxa::engine::IRuntimeSceneController
{
public:
    void FixedUpdate(const dxa::engine::RuntimeInputFrame& input) override
    {
        inputs.push_back(input);
        ++updatesSinceSample;
        if (updatesSinceSample > maxUpdatesPerSample)
        {
            maxUpdatesPerSample = updatesSinceSample;
        }
    }

    [[nodiscard]] dxa::engine::RuntimeSceneFrame SampleScene() override
    {
        ++sampleCount;
        updatesSinceSample = 0U;
        if (delayFirstSample && sampleCount == 1U)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{300});
        }
        return frame;
    }

    dxa::engine::RuntimeSceneFrame frame;
    std::vector<dxa::engine::RuntimeInputFrame> inputs;
    std::size_t sampleCount = 0U;
    std::size_t updatesSinceSample = 0U;
    std::size_t maxUpdatesPerSample = 0U;
    bool delayFirstSample = false;
};

TEST(EngineApp, RenderVerificationRejectsQuitBeforeFirstFrame)
{
    DrainThreadMessages();
    PostQuitMessage(0);

    const dxa::engine::EngineRunOptions options{
        320,
        180,
        1,
        true,
        false,
        true,
        dxa::engine::GraphicsDriver::Warp};

    EXPECT_THROW(
        (void)dxa::engine::EngineApp{}.Run(
            options, std::filesystem::path{DXA_TEST_SHADER_PATH}),
        std::runtime_error);

    DrainThreadMessages();
}

TEST(EngineApp, WritesMeasuredWarpFramesToANewBenchmarkRunDirectory)
{
    TemporaryDirectory temporary;
    const std::filesystem::path output = temporary.Path() / "run-001";
    const dxa::engine::BenchmarkRunOptions benchmark{
        output,
        0,
        2,
        20260823,
        "test-sha",
        "dxa_client --benchmark-frames 2",
        "2026-08-23T12:34:56Z"};
    const dxa::engine::EngineRunOptions options{
        96,
        54,
        2,
        true,
        false,
        false,
        dxa::engine::GraphicsDriver::Warp,
        true,
        benchmark};

    EXPECT_EQ(
        0,
        dxa::engine::EngineApp{}.Run(
            options,
            std::filesystem::path{DXA_TEST_SHADER_PATH},
            std::filesystem::path{DXA_TEST_ASSET_ROOT}));

    const std::string csv = ReadText(output / "frames.csv");
    EXPECT_EQ(3U, std::ranges::count(csv, '\n'));
    EXPECT_NE(std::string::npos, csv.find("1124"));

    const std::string json = ReadText(output / "summary.json");
    EXPECT_NE(std::string::npos, json.find("\"seed\": 20260823"));
    EXPECT_NE(std::string::npos, json.find("\"sample_count\": 2"));
    EXPECT_NE(std::string::npos, json.find("\"render_path\": \"forward\""));
    EXPECT_NE(std::string::npos, json.find("\"gpu_total_sample_count\": 2"));
    EXPECT_EQ(std::string::npos, json.find("\"adapter\": \"\""));
}

TEST(EngineApp, WritesEveryHybridPassToTheBenchmarkReport)
{
    TemporaryDirectory temporary;
    const std::filesystem::path output = temporary.Path() / "hybrid-run";
    const dxa::engine::BenchmarkRunOptions benchmark{
        output,
        0,
        2,
        20260823,
        "hybrid-test-sha",
        "dxa_client --render-path hybrid-deferred --benchmark-frames 2",
        "2026-08-23T12:34:56Z"};
    const dxa::engine::EngineRunOptions options{
        96,
        54,
        2,
        true,
        false,
        false,
        dxa::engine::GraphicsDriver::Warp,
        true,
        benchmark,
        dxa::engine::RenderPath::HybridDeferred};

    EXPECT_EQ(
        0,
        dxa::engine::EngineApp{}.Run(
            options,
            std::filesystem::path{DXA_TEST_SHADER_PATH},
            std::filesystem::path{DXA_TEST_ASSET_ROOT}));

    const std::string json = ReadText(output / "summary.json");
    EXPECT_NE(std::string::npos, json.find("\"render_path\": \"hybrid-deferred\""));
    EXPECT_NE(std::string::npos, json.find("\"gpu_total_sample_count\": 2"));
    EXPECT_NE(std::string::npos, json.find("\"gpu_shadow_sample_count\": 2"));
    EXPECT_NE(std::string::npos, json.find("\"gpu_gbuffer_sample_count\": 2"));
    EXPECT_NE(std::string::npos, json.find("\"gpu_lighting_sample_count\": 2"));
    EXPECT_NE(std::string::npos, json.find("\"gpu_transparent_sample_count\": 2"));
    EXPECT_NE(std::string::npos, json.find("\"gpu_missing_samples\": 0"));
    EXPECT_NE(std::string::npos, json.find("\"shadow_draw_calls\""));
    EXPECT_NE(std::string::npos, json.find("\"visible_object_count\""));
    EXPECT_NE(std::string::npos, json.find("\"culled_object_count\""));
}

TEST(EngineApp, RendersRuntimeSceneThroughWarp)
{
    ScriptedRuntimeScene scene;
    scene.frame.controlledPlayer = {4.0F, 0.0F, 2.0F};
    scene.frame.players[0] = {
        scene.frame.controlledPlayer,
        true};
    scene.frame.ai[0] = {{8.0F, 0.0F, 3.0F}, true};
    scene.frame.zoneRadius = 64.0F;
    scene.delayFirstSample = true;

    EXPECT_EQ(
        0,
        dxa::engine::EngineApp{}.Run(
            HiddenHybridOptions(120U),
            std::filesystem::path{DXA_TEST_SHADER_PATH},
            std::filesystem::path{DXA_TEST_ASSET_ROOT},
            &scene));
    EXPECT_GT(scene.sampleCount, 0U);
    EXPECT_GT(scene.inputs.size(), 0U);
    EXPECT_EQ(5U, scene.maxUpdatesPerSample);
}

TEST(EngineApp, RejectsRuntimeSceneWithForwardRenderer)
{
    ScriptedRuntimeScene scene;
    dxa::engine::EngineRunOptions options;
    options.renderPath = dxa::engine::RenderPath::Forward;

    EXPECT_THROW(
        (void)dxa::engine::EngineApp{}.Run(
            options,
            std::filesystem::path{DXA_TEST_SHADER_PATH},
            {},
            &scene),
        std::invalid_argument);
}

TEST(EngineApp, NullRuntimeScenePreservesForwardWarpRun)
{
    const dxa::engine::EngineRunOptions options{
        96U,
        54U,
        2U,
        true,
        false,
        true,
        dxa::engine::GraphicsDriver::Warp};

    EXPECT_EQ(
        0,
        dxa::engine::EngineApp{}.Run(
            options,
            std::filesystem::path{DXA_TEST_SHADER_PATH},
            {},
            nullptr));
}
} // namespace
