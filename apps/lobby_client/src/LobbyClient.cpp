#include <dxa/lobby_client/LobbyClient.hpp>

#include <dxa/protocol/AsioFramedConnection.hpp>
#include <dxa/protocol/LobbyMessageCodec.hpp>

#include <boost/asio/connect.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

namespace dxa::lobby_client
{
std::shared_ptr<LobbyClient> LobbyClient::Create(boost::asio::io_context& io)
{
    return std::shared_ptr<LobbyClient>{new LobbyClient{io}};
}

LobbyClient::LobbyClient(boost::asio::io_context& io)
    : io_{io},
      resolver_{io},
      socket_{io}
{
}

void LobbyClient::AsyncConnect(
    std::string host,
    const std::uint16_t port,
    LobbyClientCallbacks callbacks)
{
    if (connectStarted_)
    {
        throw std::logic_error{"lobby client connect already started"};
    }
    connectStarted_ = true;
    callbacks_ = std::move(callbacks);

    if (host.empty() || port == 0U)
    {
        const auto self = shared_from_this();
        boost::asio::post(io_, [self] {
            self->NotifyClosed(boost::asio::error::invalid_argument);
        });
        return;
    }
    Resolve(std::move(host), port);
}

std::uint32_t LobbyClient::Hello()
{
    const std::uint32_t requestId = NextRequestId();
    return Send(dxa::protocol::ClientHello{requestId});
}

std::uint32_t LobbyClient::ListRooms()
{
    const std::uint32_t requestId = NextRequestId();
    return Send(dxa::protocol::ListRoomsRequest{requestId});
}

std::uint32_t LobbyClient::CreateRoom()
{
    const std::uint32_t requestId = NextRequestId();
    return Send(dxa::protocol::CreateRoomRequest{requestId});
}

std::uint32_t LobbyClient::JoinRoom(const dxa::protocol::RoomId room)
{
    const std::uint32_t requestId = NextRequestId();
    return Send(dxa::protocol::JoinRoomRequest{requestId, room});
}

std::uint32_t LobbyClient::LeaveRoom()
{
    const std::uint32_t requestId = NextRequestId();
    return Send(dxa::protocol::LeaveRoomRequest{requestId});
}

std::uint32_t LobbyClient::SetReady(const bool ready)
{
    const std::uint32_t requestId = NextRequestId();
    return Send(dxa::protocol::SetReadyRequest{requestId, ready});
}

std::uint32_t LobbyClient::StartMatch()
{
    const std::uint32_t requestId = NextRequestId();
    return Send(dxa::protocol::StartMatchRequest{requestId});
}

void LobbyClient::Close()
{
    if (closeNotified_)
    {
        return;
    }
    resolver_.cancel();
    if (transport_)
    {
        transport_->Close();
        return;
    }

    boost::system::error_code ignored;
    socket_.cancel(ignored);
    socket_.close(ignored);
    NotifyClosed(boost::asio::error::operation_aborted);
}

void LobbyClient::Resolve(std::string host, const std::uint16_t port)
{
    const auto self = shared_from_this();
    resolver_.async_resolve(
        std::move(host),
        std::to_string(port),
        [self](
            const boost::system::error_code error,
            const boost::asio::ip::tcp::resolver::results_type endpoints) {
            if (error)
            {
                self->NotifyClosed(error);
                return;
            }
            boost::asio::async_connect(
                self->socket_,
                endpoints,
                [self](
                    const boost::system::error_code connectError,
                    const boost::asio::ip::tcp::endpoint&) {
                    if (connectError)
                    {
                        self->NotifyClosed(connectError);
                        return;
                    }
                    self->Connected();
                });
        });
}

void LobbyClient::Connected()
{
    const std::weak_ptr<LobbyClient> weak = shared_from_this();
    transport_ = dxa::protocol::AsioFramedConnection::Create(
        std::move(socket_),
        [weak](dxa::protocol::RawFrame frame) {
            if (const auto self = weak.lock())
            {
                self->Receive(frame.type, std::move(frame.payload));
            }
        },
        [weak](const boost::system::error_code error) {
            if (const auto self = weak.lock())
            {
                self->TransportClosed(error);
            }
        });
    connected_ = true;
    transport_->Start();
    if (callbacks_.onConnected)
    {
        callbacks_.onConnected();
    }
}

void LobbyClient::Receive(
    const dxa::protocol::MessageType type,
    std::vector<std::byte> payload)
{
    const auto decoded = dxa::protocol::DecodeServerMessage(type, payload);
    if (!decoded.message.has_value())
    {
        NotifyClosed(boost::asio::error::invalid_argument);
        if (transport_)
        {
            transport_->Close();
        }
        return;
    }
    if (callbacks_.onMessage)
    {
        callbacks_.onMessage(std::move(*decoded.message));
    }
}

void LobbyClient::TransportClosed(const boost::system::error_code error)
{
    connected_ = false;
    transport_.reset();
    NotifyClosed(error);
}

void LobbyClient::NotifyClosed(const boost::system::error_code error)
{
    if (closeNotified_)
    {
        return;
    }
    closeNotified_ = true;
    connected_ = false;
    if (callbacks_.onClosed)
    {
        callbacks_.onClosed(error);
    }
}

std::uint32_t LobbyClient::NextRequestId()
{
    if (!connected_ || !transport_)
    {
        throw std::logic_error{"lobby client is not connected"};
    }
    if (!nextRequestId_.has_value())
    {
        throw std::overflow_error{"lobby client request ID space exhausted"};
    }
    const std::uint32_t requestId = *nextRequestId_;
    if (requestId == std::numeric_limits<std::uint32_t>::max())
    {
        nextRequestId_.reset();
    }
    else
    {
        *nextRequestId_ = requestId + 1U;
    }
    return requestId;
}

std::uint32_t LobbyClient::Send(dxa::protocol::ClientMessage message)
{
    const std::uint32_t requestId = dxa::protocol::RequestId(message);
    if (!connected_ || !transport_)
    {
        throw std::logic_error{"lobby client is not connected"};
    }
    const auto encoded = dxa::protocol::EncodeClientMessage(message);
    if (!transport_->Send(encoded))
    {
        throw std::runtime_error{"lobby client send failed"};
    }
    return requestId;
}
} // namespace dxa::lobby_client
