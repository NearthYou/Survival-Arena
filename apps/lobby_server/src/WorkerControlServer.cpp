#include <dxa/lobby/WorkerControlServer.hpp>

#include <dxa/protocol/AsioFramedConnection.hpp>
#include <dxa/protocol/WorkerControlMessageCodec.hpp>

#include <boost/asio/socket_base.hpp>
#include <boost/asio/steady_timer.hpp>

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace dxa::lobby
{
namespace
{
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

[[nodiscard]] std::optional<dxa::protocol::ReservationId>
EventReservation(const WorkerEvent& event) noexcept
{
    if (const auto* ready = std::get_if<ReservationReadyEvent>(&event))
    {
        return ready->reservation;
    }
    if (const auto* failed = std::get_if<ReservationFailedEvent>(&event))
    {
        return failed->reservation;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<WorkerConnectionId> TimerRecipient(
    const WorkerRegistryResult& result,
    const dxa::protocol::ReservationId reservation) noexcept
{
    for (const WorkerControlOutbound& outbound : result.outbound)
    {
        if (const auto* reserve =
                std::get_if<dxa::protocol::ReserveMatch>(&outbound.message);
            reserve != nullptr && reserve->reservation == reservation)
        {
            return outbound.recipient;
        }
        if (const auto* cancel =
                std::get_if<dxa::protocol::CancelMatchReservation>(
                    &outbound.message);
            cancel != nullptr && cancel->reservation == reservation)
        {
            return outbound.recipient;
        }
    }
    return std::nullopt;
}
} // namespace

struct WorkerControlServer::State final
    : public std::enable_shared_from_this<WorkerControlServer::State>
{
    class Session final
    {
    public:
        [[nodiscard]] static std::shared_ptr<Session> Create(
            const std::shared_ptr<State>& owner,
            const WorkerConnectionId connection,
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

        [[nodiscard]] bool Send(
            const dxa::protocol::LobbyToWorkerMessage& message)
        {
            try
            {
                const auto encoded =
                    dxa::protocol::EncodeLobbyToWorkerMessage(message);
                return transport_->Send(encoded);
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

        void MarkRegistered() noexcept
        {
            registered_ = true;
        }

        [[nodiscard]] bool Registered() const noexcept
        {
            return registered_;
        }

    private:
        Session(
            const std::shared_ptr<State>& owner,
            const WorkerConnectionId connection)
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
        WorkerConnectionId connection_;
        std::shared_ptr<dxa::protocol::AsioFramedConnection> transport_;
        bool registered_ = false;
    };

    struct TimerEntry
    {
        std::shared_ptr<boost::asio::steady_timer> timer;
        WorkerConnectionId owner;
        ReservationTimerKind kind = ReservationTimerKind::Start;
    };

    State(
        boost::asio::io_context& io,
        const boost::asio::ip::tcp::endpoint endpoint,
        WorkerEventHandler eventHandler,
        const WorkerControlServerConfig config)
        : eventHandler_{std::move(eventHandler)},
          reservationTimeout_{config.reservationTimeout},
          acceptor_{io}
    {
        if (reservationTimeout_ <= std::chrono::milliseconds::zero())
        {
            throw std::invalid_argument{
                "worker reservation timeout must be positive"};
        }
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

        for (auto& [reservation, entry] : timers_)
        {
            static_cast<void>(reservation);
            entry.timer->cancel();
        }
        timers_.clear();

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

    void Execute(
        const LobbyRuntimeAction& action,
        const std::chrono::steady_clock::time_point now)
    {
        if (!stopping_)
        {
            Route(registry_.Execute(action, now));
        }
    }

    [[nodiscard]] std::uint16_t LocalPort() const
    {
        return acceptor_.local_endpoint().port();
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
                            "worker control accept failed error={}",
                            error.value());
                        self->AcceptNext();
                    }
                    return;
                }

                const auto value = TakeNext(self->nextConnection_);
                if (!value.has_value())
                {
                    boost::system::error_code ignored;
                    socket.close(ignored);
                    self->AcceptNext();
                    return;
                }

                const WorkerConnectionId connection{*value};
                auto session = Session::Create(
                    self,
                    connection,
                    std::move(socket));
                self->sessions_.emplace(connection, session);
                spdlog::info(
                    "worker_control_connection_open connection={}",
                    connection.value);
                session->Start();
                self->AcceptNext();
            });
    }

    void HandleFrame(
        const WorkerConnectionId connection,
        dxa::protocol::RawFrame frame)
    {
        const auto session = sessions_.find(connection);
        if (session == sessions_.end())
        {
            return;
        }

        const auto decoded = dxa::protocol::DecodeWorkerToLobbyMessage(
            frame.type,
            frame.payload);
        if (!decoded.message.has_value())
        {
            spdlog::warn(
                "worker control message rejected connection={} decode_error={}",
                connection.value,
                static_cast<int>(decoded.error));
            session->second->Close();
            return;
        }

        if (!session->second->Registered())
        {
            const auto* registration =
                std::get_if<dxa::protocol::WorkerRegister>(
                    &*decoded.message);
            if (registration == nullptr)
            {
                session->second->Close();
                return;
            }
            WorkerRegistryResult result = registry_.Register(
                connection,
                *registration);
            if (result.accepted)
            {
                session->second->MarkRegistered();
            }
            Route(std::move(result));
            return;
        }

        WorkerRegistryResult result = registry_.Receive(
            connection,
            *decoded.message);
        if (result.accepted)
        {
            if (const auto* cancelled =
                    std::get_if<dxa::protocol::MatchReservationCancelled>(
                        &*decoded.message))
            {
                CancelTimer(cancelled->reservation);
            }
        }
        Route(std::move(result));
    }

    void SessionClosed(
        const WorkerConnectionId connection,
        const boost::system::error_code error)
    {
        const auto session = sessions_.find(connection);
        if (session == sessions_.end())
        {
            return;
        }
        const bool registered = session->second->Registered();
        sessions_.erase(session);
        CancelTimersFor(connection);
        spdlog::info(
            "worker_control_connection_close connection={} error={}",
            connection.value,
            error.value());

        if (registered)
        {
            WorkerRegistryResult result = registry_.Disconnect(connection);
            if (!stopping_)
            {
                Route(std::move(result));
            }
        }
    }

    void Route(WorkerRegistryResult result)
    {
        for (const WorkerControlOutbound& outbound : result.outbound)
        {
            const auto session = sessions_.find(outbound.recipient);
            if (session != sessions_.end())
            {
                static_cast<void>(session->second->Send(outbound.message));
            }
        }

        for (const WorkerEvent& event : result.events)
        {
            if (const auto reservation = EventReservation(event);
                reservation.has_value())
            {
                CancelTimer(*reservation);
            }
        }
        for (const ReservationTimerDirective& directive : result.timers)
        {
            const auto recipient = TimerRecipient(
                result,
                directive.reservation);
            if (recipient.has_value())
            {
                ScheduleTimer(directive, *recipient);
            }
        }
        for (const WorkerEvent& event : result.events)
        {
            if (eventHandler_)
            {
                eventHandler_(event);
            }
        }
        for (const WorkerConnectionId connection : result.closeConnections)
        {
            const auto session = sessions_.find(connection);
            if (session != sessions_.end())
            {
                session->second->Close();
            }
        }
    }

    void ScheduleTimer(
        const ReservationTimerDirective& directive,
        const WorkerConnectionId owner)
    {
        CancelTimer(directive.reservation);
        auto timer = std::make_shared<boost::asio::steady_timer>(
            acceptor_.get_executor());
        timer->expires_after(reservationTimeout_);
        timers_.emplace(
            directive.reservation,
            TimerEntry{timer, owner, directive.kind});

        const auto self = shared_from_this();
        timer->async_wait(
            [self, timer, reservation = directive.reservation](
                const boost::system::error_code error) {
                const auto tracked = self->timers_.find(reservation);
                if (tracked == self->timers_.end()
                    || tracked->second.timer != timer)
                {
                    return;
                }
                if (error)
                {
                    self->timers_.erase(tracked);
                    return;
                }
                self->timers_.erase(tracked);
                if (!self->stopping_)
                {
                    self->Route(self->registry_.Timeout(reservation));
                }
            });
    }

    void CancelTimer(const dxa::protocol::ReservationId reservation)
    {
        const auto timer = timers_.find(reservation);
        if (timer == timers_.end())
        {
            return;
        }
        timer->second.timer->cancel();
        timers_.erase(timer);
    }

    void CancelTimersFor(const WorkerConnectionId owner)
    {
        for (auto timer = timers_.begin(); timer != timers_.end();)
        {
            if (timer->second.owner != owner)
            {
                ++timer;
                continue;
            }
            timer->second.timer->cancel();
            timer = timers_.erase(timer);
        }
    }

    WorkerRegistry registry_;
    WorkerEventHandler eventHandler_;
    std::chrono::milliseconds reservationTimeout_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::map<WorkerConnectionId, std::shared_ptr<Session>> sessions_;
    std::map<dxa::protocol::ReservationId, TimerEntry> timers_;
    std::optional<std::uint64_t> nextConnection_{1U};
    bool started_ = false;
    bool stopping_ = false;
};

WorkerControlServer::WorkerControlServer(
    boost::asio::io_context& io,
    const boost::asio::ip::tcp::endpoint endpoint,
    WorkerEventHandler eventHandler,
    const WorkerControlServerConfig config)
    : state_{std::make_shared<State>(
          io,
          endpoint,
          std::move(eventHandler),
          config)}
{
}

WorkerControlServer::~WorkerControlServer()
{
    Stop();
}

void WorkerControlServer::Start()
{
    state_->Start();
}

void WorkerControlServer::Stop()
{
    state_->Stop();
}

void WorkerControlServer::Execute(
    const LobbyRuntimeAction& action,
    const std::chrono::steady_clock::time_point now)
{
    state_->Execute(action, now);
}

std::uint16_t WorkerControlServer::LocalPort() const
{
    return state_->LocalPort();
}
} // namespace dxa::lobby
