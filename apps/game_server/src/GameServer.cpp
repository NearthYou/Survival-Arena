#include <dxa/game_server/GameServer.hpp>

#include <dxa/game_server/AuthoritativeMatch.hpp>
#include <dxa/game_server/UdpTokenSource.hpp>

#include <dxa/protocol/AsioFramedConnection.hpp>
#include <dxa/protocol/GameTcpMessageCodec.hpp>
#include <dxa/protocol/GameUdpCodec.hpp>
#include <dxa/protocol/WorkerControlMessageCodec.hpp>
#include <dxa/simulation/ArenaMap.hpp>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/steady_timer.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace dxa::game_server
{
namespace
{
using boost::asio::ip::tcp;
using boost::asio::ip::udp;

[[nodiscard]] std::optional<std::uint64_t> TakeNext(
    std::optional<std::uint64_t>& next) noexcept
{
    if (!next.has_value())
    {
        return std::nullopt;
    }
    const std::uint64_t value = *next;
    if (value == std::numeric_limits<std::uint64_t>::max())
    {
        next.reset();
    }
    else
    {
        ++*next;
    }
    return value;
}

[[nodiscard]] UdpPeer ToPeer(const udp::endpoint& endpoint)
{
    UdpPeer peer;
    peer.port = endpoint.port();
    if (endpoint.address().is_v6())
    {
        peer.ipv6 = true;
        const auto bytes = endpoint.address().to_v6().to_bytes();
        for (std::size_t index = 0U; index < bytes.size(); ++index)
        {
            peer.address[index] = static_cast<std::byte>(bytes[index]);
        }
    }
    else
    {
        const auto bytes = endpoint.address().to_v4().to_bytes();
        for (std::size_t index = 0U; index < bytes.size(); ++index)
        {
            peer.address[index] = static_cast<std::byte>(bytes[index]);
        }
    }
    return peer;
}

[[nodiscard]] udp::endpoint ToEndpoint(const UdpPeer& peer)
{
    if (peer.ipv6)
    {
        boost::asio::ip::address_v6::bytes_type bytes{};
        for (std::size_t index = 0U; index < bytes.size(); ++index)
        {
            bytes[index] = std::to_integer<unsigned char>(peer.address[index]);
        }
        return {boost::asio::ip::address_v6{bytes}, peer.port};
    }
    boost::asio::ip::address_v4::bytes_type bytes{};
    for (std::size_t index = 0U; index < bytes.size(); ++index)
    {
        bytes[index] = std::to_integer<unsigned char>(peer.address[index]);
    }
    return {boost::asio::ip::address_v4{bytes}, peer.port};
}
} // namespace

struct GameServer::State final
    : public std::enable_shared_from_this<GameServer::State>
{
    class GameSession final
    {
    public:
        [[nodiscard]] static std::shared_ptr<GameSession> Create(
            const std::shared_ptr<State>& owner,
            const GameConnectionId connection,
            tcp::socket socket)
        {
            auto session = std::shared_ptr<GameSession>{
                new GameSession{owner, connection}};
            const std::weak_ptr<GameSession> weak = session;
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

        [[nodiscard]] bool Send(
            const dxa::protocol::GameServerMessage& message)
        {
            try
            {
                return transport_->Send(
                    dxa::protocol::EncodeGameServerMessage(message));
            }
            catch (const std::exception&)
            {
                transport_->Close();
                return false;
            }
        }

        void Close()
        {
            transport_->Close();
        }

        void CloseAfterFlush()
        {
            transport_->CloseAfterFlush();
        }

        void MarkAuthenticated() noexcept
        {
            authenticated_ = true;
        }

        [[nodiscard]] bool Authenticated() const noexcept
        {
            return authenticated_;
        }

    private:
        GameSession(
            const std::shared_ptr<State>& owner,
            const GameConnectionId connection)
            : owner_{owner},
              connection_{connection}
        {
        }

        void OnFrame(dxa::protocol::RawFrame frame)
        {
            if (const auto owner = owner_.lock())
            {
                owner->HandleGameFrame(connection_, std::move(frame));
            }
        }

        void OnClosed(const boost::system::error_code error)
        {
            if (const auto owner = owner_.lock())
            {
                owner->GameSessionClosed(connection_, error);
            }
        }

        std::weak_ptr<State> owner_;
        GameConnectionId connection_;
        std::shared_ptr<dxa::protocol::AsioFramedConnection> transport_;
        bool authenticated_ = false;
    };

    State(boost::asio::io_context& io, GameServerConfig sourceConfig)
        : io_{io},
          config_{std::move(sourceConfig)},
          controlResolver_{io},
          reconnectTimer_{io},
          matchTimer_{io},
          gameAcceptor_{io},
          udpSocket_{io}
    {
        if (config_.authenticationTimeout <= std::chrono::milliseconds::zero()
            || config_.controlReconnectDelay
                <= std::chrono::milliseconds::zero()
            || config_.options.worker.value == 0U)
        {
            throw std::invalid_argument{"game server configuration is invalid"};
        }
        const auto address = boost::asio::ip::make_address(
            config_.options.gameBindAddress);

        const tcp::endpoint gameTcpEndpoint{
            address,
            config_.options.gameTcpPort};
        gameAcceptor_.open(gameTcpEndpoint.protocol());
        gameAcceptor_.set_option(
            boost::asio::socket_base::reuse_address{true});
        gameAcceptor_.bind(gameTcpEndpoint);
        gameAcceptor_.listen(boost::asio::socket_base::max_listen_connections);

        const udp::endpoint gameUdpEndpoint{
            address,
            config_.options.gameUdpPort};
        udpSocket_.open(gameUdpEndpoint.protocol());
        udpSocket_.bind(gameUdpEndpoint);
    }

    void Start()
    {
        if (started_ || stopping_)
        {
            return;
        }
        started_ = true;
        AcceptGameSession();
        ReceiveUdp();
        ConnectControl();
    }

    void Stop()
    {
        if (stopping_)
        {
            return;
        }
        stopping_ = true;
        boost::system::error_code ignored;
        controlResolver_.cancel();
        reconnectTimer_.cancel();
        matchTimer_.cancel();
        gameAcceptor_.cancel(ignored);
        gameAcceptor_.close(ignored);
        udpSocket_.cancel(ignored);
        udpSocket_.close(ignored);
        if (controlTransport_)
        {
            controlTransport_->Close();
        }
        DiscardMatch();
        CloseAllGameSessions();
    }

    [[nodiscard]] std::uint16_t GameTcpPort() const
    {
        return gameAcceptor_.local_endpoint().port();
    }

    [[nodiscard]] std::uint16_t GameUdpPort() const
    {
        return udpSocket_.local_endpoint().port();
    }

    void ConnectControl()
    {
        if (stopping_ || connectingControl_ || controlTransport_)
        {
            return;
        }
        connectingControl_ = true;
        const auto self = shared_from_this();
        controlResolver_.async_resolve(
            config_.options.lobbyControlHost,
            std::to_string(config_.options.lobbyControlPort),
            [self](
                const boost::system::error_code error,
                const tcp::resolver::results_type endpoints) {
                if (error || self->stopping_)
                {
                    self->connectingControl_ = false;
                    if (!self->stopping_)
                    {
                        self->ScheduleControlReconnect();
                    }
                    return;
                }
                auto socket = std::make_shared<tcp::socket>(self->io_);
                boost::asio::async_connect(
                    *socket,
                    endpoints,
                    [self, socket](
                        const boost::system::error_code connectError,
                        const tcp::endpoint&) {
                        self->connectingControl_ = false;
                        if (connectError || self->stopping_)
                        {
                            if (!self->stopping_)
                            {
                                self->ScheduleControlReconnect();
                            }
                            return;
                        }
                        self->AttachControl(std::move(*socket));
                    });
            });
    }

    void AttachControl(tcp::socket socket)
    {
        const std::weak_ptr<State> weak = shared_from_this();
        controlTransport_ = dxa::protocol::AsioFramedConnection::Create(
            std::move(socket),
            [weak](dxa::protocol::RawFrame frame) {
                if (const auto owner = weak.lock())
                {
                    owner->HandleControlFrame(std::move(frame));
                }
            },
            [weak](const boost::system::error_code error) {
                if (const auto owner = weak.lock())
                {
                    owner->ControlClosed(error);
                }
            });
        controlTransport_->Start();
        registered_ = false;
        SendControl(dxa::protocol::WorkerToLobbyMessage{
            dxa::protocol::WorkerRegister{
                config_.options.worker,
                config_.options.advertisedHost,
                GameTcpPort(),
                GameUdpPort(),
                1U}});
    }

    void ScheduleControlReconnect()
    {
        if (stopping_)
        {
            return;
        }
        reconnectTimer_.expires_after(config_.controlReconnectDelay);
        const auto self = shared_from_this();
        reconnectTimer_.async_wait(
            [self](const boost::system::error_code error) {
                if (!error && !self->stopping_)
                {
                    self->ConnectControl();
                }
            });
    }

    void ControlClosed(const boost::system::error_code error)
    {
        static_cast<void>(error);
        controlTransport_.reset();
        registered_ = false;
        connectingControl_ = false;
        DiscardMatch();
        CloseAllGameSessions();
        if (!stopping_)
        {
            ScheduleControlReconnect();
        }
    }

    void HandleControlFrame(dxa::protocol::RawFrame frame)
    {
        const auto decoded = dxa::protocol::DecodeLobbyToWorkerMessage(
            frame.type,
            frame.payload);
        if (!decoded.message.has_value())
        {
            controlTransport_->Close();
            return;
        }
        if (const auto* registered =
                std::get_if<dxa::protocol::WorkerRegistered>(
                    &*decoded.message))
        {
            if (registered_ || registered->worker != config_.options.worker)
            {
                controlTransport_->Close();
                return;
            }
            registered_ = true;
            return;
        }
        if (!registered_)
        {
            controlTransport_->Close();
            return;
        }

        if (const auto* reservation =
                std::get_if<dxa::protocol::ReserveMatch>(
                    &*decoded.message))
        {
            HandleReservation(*reservation);
            return;
        }
        const auto& cancellation =
            std::get<dxa::protocol::CancelMatchReservation>(
                *decoded.message);
        if (!match_.has_value()
            || activeReservation_ != cancellation.reservation
            || activeMatch_ != cancellation.match)
        {
            controlTransport_->Close();
            return;
        }
        DiscardMatch();
        CloseAllGameSessions();
        SendControl(dxa::protocol::WorkerToLobbyMessage{
            dxa::protocol::MatchReservationCancelled{
                cancellation.reservation,
                cancellation.match}});
    }

    void HandleReservation(const dxa::protocol::ReserveMatch& reservation)
    {
        if (match_.has_value())
        {
            SendControl(dxa::protocol::WorkerToLobbyMessage{
                dxa::protocol::ReserveMatchRejected{
                    reservation.reservation,
                    reservation.match,
                    dxa::protocol::WorkerReservationReject::Busy}});
            return;
        }
        try
        {
            match_.emplace(AuthoritativeMatch::Create(
                reservation,
                dxa::simulation::SurvivalArenaMapDefinition(),
                config_.matchConfig,
                tokenSource_,
                std::chrono::steady_clock::now()));
            activeReservation_ = reservation.reservation;
            activeMatch_ = reservation.match;
            activeServerTick_ = 0U;
            ++matchGeneration_;
            SendControl(dxa::protocol::WorkerToLobbyMessage{
                dxa::protocol::ReserveMatchReady{
                    reservation.reservation,
                    reservation.match}});
            ScheduleMatchTimer();
        }
        catch (const std::exception&)
        {
            match_.reset();
            activeReservation_.reset();
            activeMatch_.reset();
            SendControl(dxa::protocol::WorkerToLobbyMessage{
                dxa::protocol::ReserveMatchRejected{
                    reservation.reservation,
                    reservation.match,
                    dxa::protocol::WorkerReservationReject::SimulationInitializationFailed}});
        }
    }

    void SendControl(const dxa::protocol::WorkerToLobbyMessage& message)
    {
        if (!controlTransport_)
        {
            return;
        }
        try
        {
            static_cast<void>(controlTransport_->Send(
                dxa::protocol::EncodeWorkerToLobbyMessage(message)));
        }
        catch (const std::exception&)
        {
            controlTransport_->Close();
        }
    }

    void AcceptGameSession()
    {
        if (stopping_)
        {
            return;
        }
        const auto self = shared_from_this();
        gameAcceptor_.async_accept(
            [self](
                const boost::system::error_code error,
                tcp::socket socket) {
                if (error)
                {
                    if (!self->stopping_)
                    {
                        self->AcceptGameSession();
                    }
                    return;
                }
                const auto value = TakeNext(self->nextGameConnection_);
                if (!value.has_value())
                {
                    boost::system::error_code ignored;
                    socket.close(ignored);
                    self->AcceptGameSession();
                    return;
                }
                const GameConnectionId connection{*value};
                auto session = GameSession::Create(
                    self,
                    connection,
                    std::move(socket));
                self->gameSessions_.emplace(connection, session);
                self->StartAuthenticationTimer(connection);
                session->Start();
                self->AcceptGameSession();
            });
    }

    void StartAuthenticationTimer(const GameConnectionId connection)
    {
        auto timer = std::make_shared<boost::asio::steady_timer>(io_);
        timer->expires_after(config_.authenticationTimeout);
        authenticationTimers_.emplace(connection, timer);
        const auto self = shared_from_this();
        timer->async_wait(
            [self, timer, connection](
                const boost::system::error_code error) {
                const auto tracked = self->authenticationTimers_.find(
                    connection);
                if (tracked == self->authenticationTimers_.end()
                    || tracked->second != timer)
                {
                    return;
                }
                self->authenticationTimers_.erase(tracked);
                if (error || self->stopping_)
                {
                    return;
                }
                const auto session = self->gameSessions_.find(connection);
                if (session != self->gameSessions_.end()
                    && !session->second->Authenticated())
                {
                    session->second->Close();
                }
            });
    }

    void CancelAuthenticationTimer(const GameConnectionId connection)
    {
        const auto timer = authenticationTimers_.find(connection);
        if (timer == authenticationTimers_.end())
        {
            return;
        }
        timer->second->cancel();
        authenticationTimers_.erase(timer);
    }

    void HandleGameFrame(
        const GameConnectionId connection,
        dxa::protocol::RawFrame frame)
    {
        const auto session = gameSessions_.find(connection);
        if (session == gameSessions_.end())
        {
            return;
        }
        const auto decoded = dxa::protocol::DecodeGameClientMessage(
            frame.type,
            frame.payload);
        if (!decoded.message.has_value())
        {
            session->second->Close();
            return;
        }
        if (session->second->Authenticated())
        {
            static_cast<void>(session->second->Send(
                dxa::protocol::GameServerMessage{
                    dxa::protocol::GameServerErrorMessage{
                        dxa::protocol::GameServerErrorCode::ProtocolViolation}}));
            CancelAuthenticationTimer(connection);
            session->second->CloseAfterFlush();
            return;
        }
        if (!match_.has_value())
        {
            static_cast<void>(session->second->Send(
                dxa::protocol::GameServerMessage{
                    dxa::protocol::GameServerErrorMessage{
                        dxa::protocol::GameServerErrorCode::ServerNotReady}}));
            CancelAuthenticationTimer(connection);
            session->second->CloseAfterFlush();
            return;
        }

        const AuthoritativeMatchResult result = match_->Authenticate(
            connection,
            std::get<dxa::protocol::GameClientHello>(*decoded.message),
            std::chrono::steady_clock::now());
        const bool welcomed = std::any_of(
            result.tcp.begin(),
            result.tcp.end(),
            [connection](const GameTcpOutbound& outbound) {
                return outbound.recipient == connection
                    && std::holds_alternative<
                        dxa::protocol::GameServerWelcome>(outbound.message);
            });
        if (welcomed)
        {
            session->second->MarkAuthenticated();
            CancelAuthenticationTimer(connection);
        }
        RouteMatchResult(result);
    }

    void GameSessionClosed(
        const GameConnectionId connection,
        const boost::system::error_code error)
    {
        static_cast<void>(error);
        CancelAuthenticationTimer(connection);
        const auto session = gameSessions_.find(connection);
        if (session == gameSessions_.end())
        {
            return;
        }
        const bool authenticated = session->second->Authenticated();
        gameSessions_.erase(session);
        if (authenticated && match_.has_value() && !stopping_)
        {
            RouteMatchResult(match_->Disconnect(connection));
        }
    }

    void CloseAllGameSessions()
    {
        for (auto& [connection, timer] : authenticationTimers_)
        {
            static_cast<void>(connection);
            timer->cancel();
        }
        authenticationTimers_.clear();
        std::vector<std::shared_ptr<GameSession>> sessions;
        sessions.reserve(gameSessions_.size());
        for (const auto& [connection, session] : gameSessions_)
        {
            static_cast<void>(connection);
            sessions.push_back(session);
        }
        for (const auto& session : sessions)
        {
            session->Close();
        }
    }

    void ReceiveUdp()
    {
        if (stopping_)
        {
            return;
        }
        const auto self = shared_from_this();
        udpSocket_.async_receive_from(
            boost::asio::buffer(udpReceiveBuffer_),
            udpRemoteEndpoint_,
            [self](
                const boost::system::error_code error,
                const std::size_t received) {
                if (!self->stopping_)
                {
                    if (!error
                        && received <= dxa::protocol::MaxUdpDatagramBytes
                        && self->match_.has_value())
                    {
                        const auto decoded =
                            dxa::protocol::DecodeClientDatagram(
                                std::span{
                                    self->udpReceiveBuffer_.data(),
                                    received});
                        if (decoded.datagram.has_value())
                        {
                            self->RouteMatchResult(
                                self->match_->ReceiveClientDatagram(
                                    ToPeer(self->udpRemoteEndpoint_),
                                    *decoded.datagram));
                        }
                    }
                    self->ReceiveUdp();
                }
            });
    }

    void SendUdp(const GameUdpOutbound& outbound)
    {
        try
        {
            auto bytes = std::make_shared<std::vector<std::byte>>(
                dxa::protocol::EncodeServerDatagram(outbound.datagram).bytes);
            const udp::endpoint endpoint = ToEndpoint(outbound.recipient);
            udpSocket_.async_send_to(
                boost::asio::buffer(*bytes),
                endpoint,
                [bytes](
                    const boost::system::error_code,
                    const std::size_t) {});
        }
        catch (const std::exception&)
        {
        }
    }

    void ScheduleMatchTimer()
    {
        matchTimer_.cancel();
        if (!match_.has_value())
        {
            return;
        }
        const auto deadline = match_->NextDeadline();
        if (!deadline.has_value())
        {
            return;
        }
        matchTimer_.expires_at(*deadline);
        const std::uint64_t generation = matchGeneration_;
        const auto self = shared_from_this();
        matchTimer_.async_wait(
            [self, generation](const boost::system::error_code error) {
                if (error
                    || self->stopping_
                    || generation != self->matchGeneration_
                    || !self->match_.has_value())
                {
                    return;
                }
                self->RouteMatchResult(self->match_->Advance(
                    std::chrono::steady_clock::now()));
            });
    }

    void RouteMatchResult(const AuthoritativeMatchResult& result)
    {
        if (result.ticksExecuted
            > std::numeric_limits<std::uint32_t>::max() - activeServerTick_)
        {
            throw std::overflow_error{"adapter server tick exhausted"};
        }
        activeServerTick_ += result.ticksExecuted;
        if (result.overrun)
        {
            const std::uint64_t match = activeMatch_.has_value()
                ? activeMatch_->value
                : 0U;
            spdlog::warn(
                "game_match_overrun match={} tick={} count={} lateness_ns={}",
                match,
                activeServerTick_,
                result.totalOverruns,
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    result.overrunLateness).count());
        }

        const bool completed = std::any_of(
            result.control.begin(),
            result.control.end(),
            [](const dxa::protocol::WorkerToLobbyMessage& message) {
                return std::holds_alternative<
                    dxa::protocol::MatchFinished>(message);
            });
        if (completed)
        {
            DiscardMatch();
        }

        for (const dxa::protocol::WorkerToLobbyMessage& control
             : result.control)
        {
            SendControl(control);
        }

        std::vector<std::shared_ptr<GameSession>> closeAfterFlush;
        for (const GameTcpOutbound& outbound : result.tcp)
        {
            const auto session = gameSessions_.find(outbound.recipient);
            if (session == gameSessions_.end())
            {
                continue;
            }
            static_cast<void>(session->second->Send(outbound.message));
            if (outbound.closeAfterWrite)
            {
                CancelAuthenticationTimer(outbound.recipient);
                closeAfterFlush.push_back(session->second);
            }
        }
        for (const GameUdpOutbound& outbound : result.udp)
        {
            SendUdp(outbound);
        }
        for (const auto& session : closeAfterFlush)
        {
            session->CloseAfterFlush();
        }
        for (const GameConnectionId connection : result.closeTcp)
        {
            const auto session = gameSessions_.find(connection);
            if (session != gameSessions_.end())
            {
                session->second->Close();
            }
        }
        if (match_.has_value())
        {
            ScheduleMatchTimer();
        }
    }

    void DiscardMatch()
    {
        matchTimer_.cancel();
        match_.reset();
        activeReservation_.reset();
        activeMatch_.reset();
        activeServerTick_ = 0U;
        ++matchGeneration_;
    }

    boost::asio::io_context& io_;
    GameServerConfig config_;
    tcp::resolver controlResolver_;
    boost::asio::steady_timer reconnectTimer_;
    boost::asio::steady_timer matchTimer_;
    tcp::acceptor gameAcceptor_;
    udp::socket udpSocket_;
    std::shared_ptr<dxa::protocol::AsioFramedConnection> controlTransport_;
    std::map<GameConnectionId, std::shared_ptr<GameSession>> gameSessions_;
    std::map<GameConnectionId, std::shared_ptr<boost::asio::steady_timer>>
        authenticationTimers_;
    std::optional<AuthoritativeMatch> match_;
    std::optional<dxa::protocol::ReservationId> activeReservation_;
    std::optional<dxa::protocol::MatchId> activeMatch_;
    SecureUdpTokenSource tokenSource_;
    std::optional<std::uint64_t> nextGameConnection_{1U};
    std::array<std::byte, dxa::protocol::MaxUdpDatagramBytes + 1U>
        udpReceiveBuffer_{};
    udp::endpoint udpRemoteEndpoint_;
    std::uint64_t matchGeneration_ = 0U;
    std::uint32_t activeServerTick_ = 0U;
    bool started_ = false;
    bool stopping_ = false;
    bool connectingControl_ = false;
    bool registered_ = false;
};

GameServer::GameServer(
    boost::asio::io_context& io,
    GameServerConfig config)
    : state_{std::make_shared<State>(io, std::move(config))}
{
}

GameServer::~GameServer()
{
    Stop();
}

void GameServer::Start()
{
    state_->Start();
}

void GameServer::Stop()
{
    state_->Stop();
}

std::uint16_t GameServer::GameTcpPort() const
{
    return state_->GameTcpPort();
}

std::uint16_t GameServer::GameUdpPort() const
{
    return state_->GameUdpPort();
}
} // namespace dxa::game_server
