#pragma once

#include <dxa/lobby/LobbyService.hpp>
#include <dxa/lobby/WorkerControlServer.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <memory>

namespace dxa::lobby
{
class LobbyTcpServer
{
public:
    LobbyTcpServer(
        boost::asio::io_context& io,
        LobbyService& service,
        boost::asio::ip::tcp::endpoint endpoint,
        boost::asio::ip::tcp::endpoint workerEndpoint,
        WorkerControlServerConfig workerConfig = {});
    ~LobbyTcpServer();

    LobbyTcpServer(const LobbyTcpServer&) = delete;
    LobbyTcpServer& operator=(const LobbyTcpServer&) = delete;

    void Start();
    void Stop();
    [[nodiscard]] std::uint16_t LocalPort() const;
    [[nodiscard]] std::uint16_t WorkerControlPort() const;

private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace dxa::lobby
