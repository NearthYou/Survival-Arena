#pragma once

#include <dxa/bot_client/BotClientOptions.hpp>
#include <dxa/protocol/GameTcpMessages.hpp>

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace dxa::bot_client
{
struct BotCoordinatorTimeouts
{
    std::chrono::milliseconds lobby{std::chrono::seconds{30}};
    std::chrono::milliseconds game{std::chrono::minutes{11}};
};

struct BotSessionReport
{
    dxa::protocol::PlayerId player;
    std::optional<dxa::protocol::MatchId> match;
    std::uint64_t snapshotsApplied = 0U;
    std::uint64_t keyframesApplied = 0U;
    std::uint64_t deltasApplied = 0U;
    std::uint64_t receivedTcpBytes = 0U;
    std::uint64_t receivedUdpBytes = 0U;
    std::uint64_t discardedSnapshots = 0U;
    std::uint64_t keyframeRequests = 0U;
    std::uint64_t udpDatagramsDropped = 0U;
    std::uint64_t udpDatagramsDelayed = 0U;
    std::uint64_t udpDatagramsDelivered = 0U;
    std::uint64_t shapedQueueOverflows = 0U;
    std::uint64_t measurementNanoseconds = 0U;
    int exitCode = 0;
};

struct BotCoordinatorReport
{
    std::vector<BotSessionReport> sessions;
    std::optional<dxa::protocol::GameMatchResult> result;
    int exitCode = 0;
};

class BotCoordinator
{
public:
    BotCoordinator(
        boost::asio::io_context& io,
        BotClientOptions options,
        BotCoordinatorTimeouts timeouts = {});
    ~BotCoordinator();
    BotCoordinator(const BotCoordinator&) = delete;
    BotCoordinator& operator=(const BotCoordinator&) = delete;

    void Start();
    [[nodiscard]] bool GameAuthenticated() const noexcept;
    [[nodiscard]] std::uint64_t SnapshotCount() const noexcept;
    [[nodiscard]] std::optional<dxa::protocol::GameMatchResult>
    Result() const;
    [[nodiscard]] BotCoordinatorReport Report() const;
    [[nodiscard]] int ExitCode() const noexcept;
    [[nodiscard]] bool Done() const noexcept;
    void Stop();

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
} // namespace dxa::bot_client
