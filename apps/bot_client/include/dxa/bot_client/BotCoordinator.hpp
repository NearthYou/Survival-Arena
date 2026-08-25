#pragma once

#include <dxa/bot_client/BotClientOptions.hpp>
#include <dxa/protocol/GameTcpMessages.hpp>

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>

namespace dxa::bot_client
{
struct BotCoordinatorTimeouts
{
    std::chrono::milliseconds lobby{std::chrono::seconds{30}};
    std::chrono::milliseconds game{std::chrono::minutes{11}};
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
    [[nodiscard]] int ExitCode() const noexcept;
    [[nodiscard]] bool Done() const noexcept;
    void Stop();

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
} // namespace dxa::bot_client
