#pragma once

#include <boost/asio/io_context.hpp>

#include <memory>

namespace dxa::game_client
{
class GameSession;

class GameNetworkRuntime
{
public:
    GameNetworkRuntime();
    ~GameNetworkRuntime();
    GameNetworkRuntime(const GameNetworkRuntime&) = delete;
    GameNetworkRuntime& operator=(const GameNetworkRuntime&) = delete;

    [[nodiscard]] bool Start();
    void Stop();

private:
    struct Impl;

    [[nodiscard]] boost::asio::io_context& Io() noexcept;
    [[nodiscard]] bool Started() const noexcept;
    [[nodiscard]] bool RunningOnThisThread() const noexcept;

    std::shared_ptr<Impl> impl_;

    friend class GameSession;
};
} // namespace dxa::game_client
