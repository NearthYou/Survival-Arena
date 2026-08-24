#include "support/game_network_fixture.hpp"

#include <dxa/bot_client/BotCoordinator.hpp>
#include <dxa/client/NetworkClientController.hpp>
#include <dxa/engine/EngineApp.hpp>
#include <dxa/game_server/UdpTokenSource.hpp>

#include <gtest/gtest.h>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace
{
using namespace std::chrono_literals;

class DeterministicVerticalTokenSource final
    : public dxa::game_server::IUdpTokenSource
{
public:
    [[nodiscard]] bool Fill(
        const std::span<std::byte, 16U> output) noexcept override
    {
        const std::uint8_t seed = next_.fetch_add(1U);
        for (std::size_t index = 0U; index < output.size(); ++index)
        {
            output[index] = static_cast<std::byte>(
                static_cast<std::uint8_t>(seed + index));
        }
        return true;
    }

    [[nodiscard]] std::uint32_t FillCount() const noexcept
    {
        return static_cast<std::uint32_t>(next_.load() - 0x41U);
    }

private:
    std::atomic<std::uint8_t> next_{0x41U};
};

class ScopedOutputCapture
{
public:
    ScopedOutputCapture()
    {
        testing::internal::CaptureStdout();
        testing::internal::CaptureStderr();
    }

    ~ScopedOutputCapture()
    {
        if (active_)
        {
            Finish();
        }
    }

    void Finish()
    {
        if (!active_)
        {
            return;
        }
        captured_ = testing::internal::GetCapturedStderr()
            + testing::internal::GetCapturedStdout();
        active_ = false;
    }

    [[nodiscard]] const std::string& Text() const noexcept
    {
        return captured_;
    }

private:
    std::string captured_;
    bool active_ = true;
};

class VerticalWatchdog
{
public:
    explicit VerticalWatchdog(const std::chrono::seconds timeout)
        : ownerThread_{GetCurrentThreadId()},
          thread_{[this, timeout] {
              std::unique_lock lock{mutex_};
              if (!condition_.wait_for(lock, timeout, [this] { return done_; }))
              {
                  timedOut_.store(true);
                  static_cast<void>(PostThreadMessageW(
                      ownerThread_,
                      WM_QUIT,
                      0,
                      0));
              }
          }}
    {
    }

    ~VerticalWatchdog()
    {
        Finish();
    }

    void Finish()
    {
        {
            std::scoped_lock lock{mutex_};
            done_ = true;
        }
        condition_.notify_all();
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

    [[nodiscard]] bool TimedOut() const noexcept
    {
        return timedOut_.load();
    }

private:
    DWORD ownerThread_ = 0U;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> timedOut_{false};
    bool done_ = false;
    std::thread thread_;
};

[[nodiscard]] dxa::simulation::MatchConfig ShortMatchConfig()
{
    dxa::simulation::MatchConfig config =
        dxa::simulation::DefaultMatchConfig();
    config.meleeNeutralCount = 0U;
    config.rangedNeutralCount = 0U;
    config.rifleLootCount = 0U;
    config.arcPulseLootCount = 0U;
    config.medKitLootCount = 0U;
    config.suddenDeathTick = 30U;
    config.hardTimeoutTick = 60U;
    return config;
}

[[nodiscard]] std::string SecretHex(const std::uint8_t seed)
{
    std::ostringstream encoded;
    encoded << std::hex << std::setfill('0');
    for (std::uint8_t index = 0U; index < 16U; ++index)
    {
        encoded << std::setw(2)
                << static_cast<std::uint32_t>(
                    static_cast<std::uint8_t>(seed + index));
    }
    return encoded.str();
}

class VerticalFixture
{
public:
    explicit VerticalFixture(dxa::simulation::MatchConfig matchConfig)
        : tokens_{std::make_shared<DeterministicVerticalTokenSource>()},
          network_{std::move(matchConfig), tokens_}
    {
    }

    ~VerticalFixture()
    {
        StopServers();
    }

    void StartServers()
    {
        network_.StartLobbyAndWorker();
        botWork_.emplace(boost::asio::make_work_guard(network_.BotIo()));
        botThread_ = std::thread{[this] { network_.BotIo().run(); }};
    }

    void StopServers()
    {
        if (stopped_.exchange(true))
        {
            return;
        }
        network_.StopWorker();
        botWork_.reset();
        network_.BotIo().stop();
        if (botThread_.joinable())
        {
            botThread_.join();
        }
    }

    [[nodiscard]] dxa::client::NetworkClientOptions HostOptions(
        const std::uint8_t expectedPlayers) const
    {
        return dxa::client::NetworkClientOptions{
            "127.0.0.1",
            network_.LobbyPort(),
            expectedPlayers};
    }

    void WaitForRoom(dxa::client::NetworkClientController& host)
    {
        WaitUntil([&host] { return host.Room().has_value(); });
    }

    [[nodiscard]] boost::asio::io_context& BotIo() noexcept
    {
        return network_.BotIo();
    }

    [[nodiscard]] dxa::bot_client::BotClientOptions PlayBotOptions(
        const dxa::protocol::RoomId room) const
    {
        dxa::bot_client::BotClientOptions options;
        options.port = network_.LobbyPort();
        options.room = room;
        options.play = true;
        return options;
    }

    [[nodiscard]] dxa::engine::EngineRunOptions HiddenWarpHybridOptions(
        const std::uint32_t maximumFrames) const
    {
        return dxa::engine::EngineRunOptions{
            320U,
            180U,
            maximumFrames,
            true,
            true,
            true,
            dxa::engine::GraphicsDriver::Warp,
            true,
            std::nullopt,
            dxa::engine::RenderPath::HybridDeferred};
    }

    void WaitForResults(
        dxa::client::NetworkClientController& host,
        const dxa::bot_client::BotCoordinator& bot)
    {
        WaitUntil([&] {
            host.FixedUpdate({});
            return host.Result().has_value()
                && bot.Result().has_value()
                && host.SnapshotCount() >= 2U
                && bot.SnapshotCount() >= 2U;
        });
    }

    void SetCapturedOutput(std::string output)
    {
        capturedOutput_ = std::move(output);
    }

    [[nodiscard]] std::size_t SecretLeakCount() const
    {
        std::string normalized = capturedOutput_;
        std::transform(
            normalized.begin(),
            normalized.end(),
            normalized.begin(),
            [](const unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
        const std::array secrets{
            SecretHex(0x01U),
            SecretHex(0x02U),
            SecretHex(0x41U),
            SecretHex(0x42U)};
        return static_cast<std::size_t>(std::count_if(
            secrets.begin(),
            secrets.end(),
            [&normalized](const std::string& secret) {
                return normalized.find(secret) != std::string::npos;
            }));
    }

    [[nodiscard]] std::uint32_t TokenFillCount() const noexcept
    {
        return tokens_->FillCount();
    }

    [[nodiscard]] std::filesystem::path ShaderPath() const
    {
        return std::filesystem::path{DXA_TEST_SHADER_PATH};
    }

    [[nodiscard]] std::filesystem::path AssetRoot() const
    {
        return std::filesystem::path{DXA_TEST_ASSET_ROOT};
    }

private:
    template <typename Condition>
    void WaitUntil(Condition condition)
    {
        const auto deadline = std::chrono::steady_clock::now() + 15s;
        while (!condition())
        {
            if (std::chrono::steady_clock::now() >= deadline)
            {
                throw std::runtime_error{
                    "network vertical slice watchdog expired"};
            }
            std::this_thread::sleep_for(10ms);
        }
    }

    std::shared_ptr<DeterministicVerticalTokenSource> tokens_;
    dxa::test::GameNetworkFixture network_;
    std::optional<boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type>> botWork_;
    std::thread botThread_;
    std::atomic<bool> stopped_{false};
    std::string capturedOutput_;
};
} // namespace

TEST(NetworkVerticalSlice, WarpClientAndHeadlessBotShareOneMatch)
{
    const auto startedAt = std::chrono::steady_clock::now();
    ScopedOutputCapture capture;
    VerticalFixture fixture{ShortMatchConfig()};
    fixture.StartServers();
    dxa::client::NetworkClientController host{fixture.HostOptions(2U)};
    host.Start();
    fixture.WaitForRoom(host);
    dxa::bot_client::BotCoordinator bot{
        fixture.BotIo(),
        fixture.PlayBotOptions(*host.Room())};
    bot.Start();

    VerticalWatchdog watchdog{15s};
    EXPECT_EQ(0, dxa::engine::EngineApp{}.Run(
        fixture.HiddenWarpHybridOptions(300U),
        fixture.ShaderPath(),
        fixture.AssetRoot(),
        &host));
    watchdog.Finish();
    EXPECT_FALSE(watchdog.TimedOut());
    fixture.WaitForResults(host, bot);

    ASSERT_TRUE(host.Result().has_value());
    ASSERT_TRUE(bot.Result().has_value());
    EXPECT_EQ(*bot.Result(), *host.Result());
    EXPECT_EQ(60U, host.Result()->finishedTick);
    EXPECT_GE(host.SnapshotCount(), 2U);
    EXPECT_GE(bot.SnapshotCount(), 2U);
    EXPECT_EQ(2U, fixture.TokenFillCount());

    const dxa::protocol::RoomId room = *host.Room();
    const dxa::protocol::MatchId match = host.Result()->match;
    const std::uint32_t finishedTick = host.Result()->finishedTick;
    const std::uint64_t hostSnapshots = host.SnapshotCount();
    const std::uint64_t botSnapshots = bot.SnapshotCount();

    bot.Stop();
    host.Stop();
    fixture.StopServers();
    capture.Finish();
    fixture.SetCapturedOutput(capture.Text());
    EXPECT_EQ(0U, fixture.SecretLeakCount());

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt);
    std::cout << "network vertical evidence: room=" << room.value
              << " match=" << match.value
              << " host_snapshots=" << hostSnapshots
              << " bot_snapshots=" << botSnapshots
              << " finished_tick=" << finishedTick
              << " elapsed_ms=" << elapsed.count() << '\n';
}
