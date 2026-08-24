#pragma once

#include <dxa/lobby/LobbyService.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

namespace dxa::lobby
{
using LobbyRuntimeActionHandler = std::function<void(
    const LobbyRuntimeAction&)>;

class LobbyTcpServer
{
public:
    LobbyTcpServer(
        boost::asio::io_context& io,
        LobbyService& service,
        boost::asio::ip::tcp::endpoint endpoint);
    ~LobbyTcpServer();

    LobbyTcpServer(const LobbyTcpServer&) = delete;
    LobbyTcpServer& operator=(const LobbyTcpServer&) = delete;

    void Start();
    void Stop();
    void SetRuntimeActionHandler(LobbyRuntimeActionHandler handler);
    void ApplyWorkerEvent(
        const WorkerEvent& event,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::uint16_t LocalPort() const;

private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace dxa::lobby
