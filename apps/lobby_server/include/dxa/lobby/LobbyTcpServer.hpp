#pragma once

#include <dxa/lobby/LobbyService.hpp>

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
        boost::asio::ip::tcp::endpoint endpoint);
    ~LobbyTcpServer();

    LobbyTcpServer(const LobbyTcpServer&) = delete;
    LobbyTcpServer& operator=(const LobbyTcpServer&) = delete;

    void Start();
    void Stop();
    [[nodiscard]] std::uint16_t LocalPort() const;

private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace dxa::lobby
