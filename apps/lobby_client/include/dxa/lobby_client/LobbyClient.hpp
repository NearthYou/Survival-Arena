#pragma once

#include <dxa/protocol/LobbyMessages.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dxa::protocol
{
class AsioFramedConnection;
}

namespace dxa::lobby_client
{
struct LobbyClientCallbacks
{
    std::function<void()> onConnected;
    std::function<void(dxa::protocol::ServerMessage)> onMessage;
    std::function<void(boost::system::error_code)> onClosed;
};

class LobbyClient final : public std::enable_shared_from_this<LobbyClient>
{
public:
    [[nodiscard]] static std::shared_ptr<LobbyClient> Create(
        boost::asio::io_context& io);

    void AsyncConnect(
        std::string host,
        std::uint16_t port,
        LobbyClientCallbacks callbacks);
    [[nodiscard]] std::uint32_t Hello();
    [[nodiscard]] std::uint32_t ListRooms();
    [[nodiscard]] std::uint32_t CreateRoom();
    [[nodiscard]] std::uint32_t JoinRoom(dxa::protocol::RoomId room);
    [[nodiscard]] std::uint32_t LeaveRoom();
    [[nodiscard]] std::uint32_t SetReady(bool ready);
    [[nodiscard]] std::uint32_t StartMatch();
    void Close();

private:
    explicit LobbyClient(boost::asio::io_context& io);

    void Resolve(std::string host, std::uint16_t port);
    void Connected();
    void Receive(dxa::protocol::MessageType type, std::vector<std::byte> payload);
    void TransportClosed(boost::system::error_code error);
    void NotifyClosed(boost::system::error_code error);
    [[nodiscard]] std::uint32_t NextRequestId();
    [[nodiscard]] std::uint32_t Send(dxa::protocol::ClientMessage message);

    boost::asio::io_context& io_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::socket socket_;
    std::shared_ptr<dxa::protocol::AsioFramedConnection> transport_;
    LobbyClientCallbacks callbacks_;
    std::optional<std::uint32_t> nextRequestId_{1U};
    bool connectStarted_ = false;
    bool connected_ = false;
    bool closeNotified_ = false;
};
} // namespace dxa::lobby_client
