#pragma once

#include <dxa/lobby/WorkerRegistry.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

namespace dxa::lobby
{
struct WorkerControlServerConfig
{
    std::chrono::milliseconds reservationTimeout{2000};
};

using WorkerEventHandler = std::function<void(WorkerEvent)>;

class WorkerControlServer
{
public:
    WorkerControlServer(
        boost::asio::io_context& io,
        boost::asio::ip::tcp::endpoint endpoint,
        WorkerEventHandler eventHandler,
        WorkerControlServerConfig config = {});
    ~WorkerControlServer();

    WorkerControlServer(const WorkerControlServer&) = delete;
    WorkerControlServer& operator=(const WorkerControlServer&) = delete;

    void Start();
    void Stop();
    void Execute(
        const LobbyRuntimeAction& action,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::uint16_t LocalPort() const;

private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace dxa::lobby
