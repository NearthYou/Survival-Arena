#include <dxa/lobby/LobbyTcpServer.hpp>

#include <dxa/protocol/AsioFramedConnection.hpp>
#include <dxa/protocol/LobbyMessageCodec.hpp>

#include <boost/asio/socket_base.hpp>
#include <boost/asio/post.hpp>

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace dxa::lobby
{
namespace
{
[[nodiscard]] std::string_view AuditTypeName(
    const LobbyAuditEventType type) noexcept
{
    switch (type)
    {
    case LobbyAuditEventType::PlayerAssigned:
        return "player_assigned";
    case LobbyAuditEventType::RoomCreated:
        return "room_created";
    case LobbyAuditEventType::RoomDeleted:
        return "room_deleted";
    case LobbyAuditEventType::HostTransferred:
        return "host_transferred";
    case LobbyAuditEventType::MatchStarted:
        return "match_started";
    case LobbyAuditEventType::StartFailed:
        return "start_failed";
    }
    return "unknown";
}

void LogAudit(const LobbyAuditEvent& event)
{
    spdlog::info(
        "lobby_audit type={} connection={} player={} room={} match={} error={} endpoint_host={} endpoint_tcp={} endpoint_udp={}",
        AuditTypeName(event.type),
        event.connection.has_value() ? event.connection->value : 0U,
        event.player.has_value() ? event.player->value : 0U,
        event.room.has_value() ? event.room->value : 0U,
        event.match.has_value() ? event.match->value : 0U,
        event.error.has_value()
            ? static_cast<std::uint16_t>(*event.error)
            : 0U,
        event.endpoint.has_value() ? event.endpoint->host : "",
        event.endpoint.has_value() ? event.endpoint->tcpPort : 0U,
        event.endpoint.has_value() ? event.endpoint->udpPort : 0U);
}
} // namespace

struct LobbyTcpServer::State final
    : public std::enable_shared_from_this<LobbyTcpServer::State>
{
    class Session final
    {
    public:
        [[nodiscard]] static std::shared_ptr<Session> Create(
            const std::shared_ptr<State>& owner,
            const ConnectionId connection,
            boost::asio::ip::tcp::socket socket)
        {
            auto session = std::shared_ptr<Session>{
                new Session{owner, connection}};
            const std::weak_ptr<Session> weak = session;
            session->transport_ = dxa::protocol::AsioFramedConnection::Create(
                std::move(socket),
                [weak](dxa::protocol::RawFrame frame) {
                    if (const auto locked = weak.lock())
                    {
                        locked->OnFrame(std::move(frame));
                    }
                },
                [weak](const boost::system::error_code error) {
                    if (const auto locked = weak.lock())
                    {
                        locked->OnClosed(error);
                    }
                });
            return session;
        }

        void Start()
        {
            transport_->Start();
        }

        void Send(const dxa::protocol::ServerMessage& message)
        {
            try
            {
                const auto encoded = dxa::protocol::EncodeServerMessage(message);
                static_cast<void>(transport_->Send(encoded));
            }
            catch (const std::exception&)
            {
                spdlog::error(
                    "lobby message encode failed connection={} error=InternalError",
                    connection_.value);
                transport_->Close();
            }
        }

        void Close()
        {
            transport_->Close();
        }

    private:
        Session(
            const std::shared_ptr<State>& owner,
            const ConnectionId connection)
            : owner_{owner},
              connection_{connection}
        {
        }

        void OnFrame(dxa::protocol::RawFrame frame)
        {
            if (const auto owner = owner_.lock())
            {
                owner->HandleFrame(connection_, std::move(frame));
            }
        }

        void OnClosed(const boost::system::error_code error)
        {
            if (const auto owner = owner_.lock())
            {
                owner->SessionClosed(connection_, error);
            }
        }

        std::weak_ptr<State> owner_;
        ConnectionId connection_;
        std::shared_ptr<dxa::protocol::AsioFramedConnection> transport_;
    };

    State(
        boost::asio::io_context& io,
        LobbyService& service,
        const boost::asio::ip::tcp::endpoint endpoint)
        : service_{service},
          acceptor_{io}
    {
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::socket_base::reuse_address{true});
        acceptor_.bind(endpoint);
        acceptor_.listen(boost::asio::socket_base::max_listen_connections);
    }

    void Start()
    {
        if (started_ || stopping_)
        {
            return;
        }
        started_ = true;
        AcceptNext();
    }

    void Stop()
    {
        if (stopping_)
        {
            return;
        }
        stopping_ = true;

        boost::system::error_code ignored;
        acceptor_.cancel(ignored);
        acceptor_.close(ignored);

        std::vector<std::shared_ptr<Session>> sessions;
        sessions.reserve(sessions_.size());
        for (const auto& [connection, session] : sessions_)
        {
            static_cast<void>(connection);
            sessions.push_back(session);
        }
        for (const auto& session : sessions)
        {
            session->Close();
        }
    }

    [[nodiscard]] std::uint16_t LocalPort() const
    {
        return acceptor_.local_endpoint().port();
    }

    void SetRuntimeActionHandler(LobbyRuntimeActionHandler handler)
    {
        actionHandler_ = std::move(handler);
    }

    void ApplyWorkerEvent(
        WorkerEvent event,
        const std::chrono::steady_clock::time_point now)
    {
        const auto self = shared_from_this();
        boost::asio::post(
            acceptor_.get_executor(),
            [self, event = std::move(event), now] {
                if (!self->stopping_)
                {
                    self->Route(self->service_.HandleWorkerEvent(event, now));
                }
            });
    }

    void AcceptNext()
    {
        if (stopping_)
        {
            return;
        }
        const auto self = shared_from_this();
        acceptor_.async_accept(
            [self](
                const boost::system::error_code error,
                boost::asio::ip::tcp::socket socket) {
                if (error)
                {
                    if (!self->stopping_)
                    {
                        spdlog::warn(
                            "lobby accept failed error={}",
                            error.value());
                        self->AcceptNext();
                    }
                    return;
                }

                const auto connection = self->service_.OpenConnection();
                if (!connection.has_value())
                {
                    boost::system::error_code ignored;
                    socket.close(ignored);
                    self->AcceptNext();
                    return;
                }

                auto session = Session::Create(
                    self,
                    *connection,
                    std::move(socket));
                self->sessions_.emplace(*connection, session);
                spdlog::info(
                    "lobby_connection_open connection={}",
                    connection->value);
                session->Start();
                self->AcceptNext();
            });
    }

    void HandleFrame(
        const ConnectionId connection,
        dxa::protocol::RawFrame frame)
    {
        const auto decoded = dxa::protocol::DecodeClientMessage(
            frame.type,
            frame.payload);
        if (!decoded.message.has_value())
        {
            spdlog::warn(
                "lobby client message rejected connection={} decode_error={}",
                connection.value,
                static_cast<int>(decoded.error));
            const auto session = sessions_.find(connection);
            if (session != sessions_.end())
            {
                session->second->Close();
            }
            return;
        }

        Route(service_.Handle(
            connection,
            *decoded.message,
            std::chrono::steady_clock::now()));
    }

    void SessionClosed(
        const ConnectionId connection,
        const boost::system::error_code error)
    {
        const auto session = sessions_.find(connection);
        if (session == sessions_.end())
        {
            return;
        }
        sessions_.erase(session);
        spdlog::info(
            "lobby_connection_close connection={} error={}",
            connection.value,
            error.value());
        Route(service_.Disconnect(connection));
    }

    void Route(const LobbyServiceResult& result)
    {
        for (const OutboundMessage& outbound : result.outbound)
        {
            const auto session = sessions_.find(outbound.recipient);
            if (session != sessions_.end())
            {
                session->second->Send(outbound.message);
            }
        }
        for (const LobbyAuditEvent& event : result.audit)
        {
            LogAudit(event);
        }
        for (const LobbyRuntimeAction& action : result.actions)
        {
            if (actionHandler_)
            {
                actionHandler_(action);
                continue;
            }
            if (const auto* reserve = std::get_if<ReserveMatchAction>(&action))
            {
                Route(service_.HandleWorkerEvent(
                    ReservationFailedEvent{
                        reserve->reservation,
                        reserve->match},
                    std::chrono::steady_clock::now()));
            }
        }
    }

    LobbyService& service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::map<ConnectionId, std::shared_ptr<Session>> sessions_;
    LobbyRuntimeActionHandler actionHandler_;
    bool started_ = false;
    bool stopping_ = false;
};

LobbyTcpServer::LobbyTcpServer(
    boost::asio::io_context& io,
    LobbyService& service,
    const boost::asio::ip::tcp::endpoint endpoint)
    : state_{std::make_shared<State>(io, service, endpoint)}
{
}

LobbyTcpServer::~LobbyTcpServer()
{
    Stop();
}

void LobbyTcpServer::Start()
{
    state_->Start();
}

void LobbyTcpServer::Stop()
{
    state_->Stop();
}

void LobbyTcpServer::SetRuntimeActionHandler(
    LobbyRuntimeActionHandler handler)
{
    state_->SetRuntimeActionHandler(std::move(handler));
}

void LobbyTcpServer::ApplyWorkerEvent(
    const WorkerEvent& event,
    const std::chrono::steady_clock::time_point now)
{
    state_->ApplyWorkerEvent(event, now);
}

std::uint16_t LobbyTcpServer::LocalPort() const
{
    return state_->LocalPort();
}
} // namespace dxa::lobby
