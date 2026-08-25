#include <dxa/game_server/GameServer.hpp>

#include <dxa/protocol/AsioFramedConnection.hpp>
#include <dxa/protocol/GameTcpMessageCodec.hpp>
#include <dxa/protocol/LobbyMessageCodec.hpp>
#include <dxa/protocol/WorkerControlMessageCodec.hpp>

#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using namespace std::chrono_literals;
using boost::asio::ip::tcp;
using dxa::protocol::AsioFramedConnection;
using dxa::protocol::GameClientHello;
using dxa::protocol::GameServerErrorCode;
using dxa::protocol::GameServerErrorMessage;
using dxa::protocol::GameServerMessage;
using dxa::protocol::GameServerWelcome;
using dxa::protocol::LobbyToWorkerMessage;
using dxa::protocol::MatchId;
using dxa::protocol::MatchTicketValue;
using dxa::protocol::PlayerId;
using dxa::protocol::ReserveMatch;
using dxa::protocol::ReservedParticipant;
using dxa::protocol::WorkerId;
using dxa::protocol::WorkerRegister;
using dxa::protocol::WorkerRegistered;
using dxa::protocol::WorkerToLobbyMessage;

[[nodiscard]] MatchTicketValue Ticket(const std::uint8_t seed)
{
    MatchTicketValue ticket;
    for (std::size_t index = 0U; index < ticket.size(); ++index)
    {
        ticket[index] = static_cast<std::byte>(
            static_cast<std::uint8_t>(seed + index));
    }
    return ticket;
}

void RunUntil(
    boost::asio::io_context& io,
    const std::function<bool()>& condition)
{
    bool timedOut = false;
    boost::asio::steady_timer timer{io};
    timer.expires_after(5s);
    timer.async_wait([&timedOut](const boost::system::error_code error) {
        if (!error)
        {
            timedOut = true;
        }
    });

    io.restart();
    while (!condition() && !timedOut)
    {
        if (io.run_one() == 0U)
        {
            break;
        }
    }
    timer.cancel();
    io.restart();
    while (io.poll_one() != 0U)
    {
    }
    if (!condition())
    {
        throw std::runtime_error{"game server adapter test timed out"};
    }
}

class FakeLobbyControl
{
public:
    explicit FakeLobbyControl(boost::asio::io_context& io)
        : acceptor_{
              io,
              tcp::endpoint{
                  boost::asio::ip::make_address("127.0.0.1"),
                  0U}}
    {
    }

    void Start()
    {
        acceptor_.async_accept([this](
            const boost::system::error_code error,
            tcp::socket socket) {
            if (error)
            {
                return;
            }
            transport_ = AsioFramedConnection::Create(
                std::move(socket),
                [this](dxa::protocol::RawFrame frame) {
                    const auto decoded =
                        dxa::protocol::DecodeWorkerToLobbyMessage(
                            frame.type,
                            frame.payload);
                    if (!decoded.message.has_value())
                    {
                        protocolError_ = true;
                        return;
                    }
                    messages_.push_back(*decoded.message);
                    if (const auto* registration =
                            std::get_if<WorkerRegister>(
                                &messages_.back()))
                    {
                        Send(LobbyToWorkerMessage{
                            WorkerRegistered{registration->worker}});
                    }
                },
                [this](const boost::system::error_code error) {
                    closedError_ = error;
                });
            transport_->Start();
        });
    }

    void Stop()
    {
        boost::system::error_code ignored;
        acceptor_.close(ignored);
        if (transport_)
        {
            transport_->Close();
        }
    }

    [[nodiscard]] std::uint16_t Port() const
    {
        return acceptor_.local_endpoint().port();
    }

    void SendReservation()
    {
        const std::vector<ReservedParticipant> participants{
            {PlayerId{1U}, Ticket(1U)},
            {PlayerId{2U}, Ticket(2U)}};
        Send(LobbyToWorkerMessage{ReserveMatch{
            dxa::protocol::ReservationId{1U},
            MatchId{7U},
            20260824U,
            60000U,
            participants}});
    }

    template <typename Message>
    [[nodiscard]] const Message* Latest() const
    {
        for (auto message = messages_.rbegin();
             message != messages_.rend();
             ++message)
        {
            if (const auto* value = std::get_if<Message>(&*message))
            {
                return value;
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool ProtocolError() const noexcept
    {
        return protocolError_;
    }

private:
    void Send(const LobbyToWorkerMessage& message)
    {
        if (!transport_
            || !transport_->Send(
                dxa::protocol::EncodeLobbyToWorkerMessage(message)))
        {
            throw std::runtime_error{"fake control send failed"};
        }
    }

    tcp::acceptor acceptor_;
    std::shared_ptr<AsioFramedConnection> transport_;
    std::vector<WorkerToLobbyMessage> messages_;
    std::optional<boost::system::error_code> closedError_;
    bool protocolError_ = false;
};

class GameProbe
{
public:
    GameProbe(
        boost::asio::io_context& io,
        const std::uint16_t port)
        : state_{std::make_shared<State>()}
    {
        tcp::socket socket{io};
        socket.connect({
            boost::asio::ip::make_address("127.0.0.1"),
            port});
        const std::weak_ptr<State> weak = state_;
        transport_ = AsioFramedConnection::Create(
            std::move(socket),
            [weak](dxa::protocol::RawFrame frame) {
                if (const auto state = weak.lock())
                {
                    const auto decoded =
                        dxa::protocol::DecodeGameServerMessage(
                            frame.type,
                            frame.payload);
                    if (decoded.message.has_value())
                    {
                        state->messages.push_back(*decoded.message);
                    }
                    else
                    {
                        state->protocolError = true;
                    }
                }
            },
            [weak](const boost::system::error_code error) {
                if (const auto state = weak.lock())
                {
                    state->closedError = error;
                }
            });
        transport_->Start();
    }

    ~GameProbe()
    {
        transport_->Close();
    }

    void SendHello(const PlayerId player, const MatchTicketValue& ticket)
    {
        Send(dxa::protocol::EncodeGameClientMessage(
            dxa::protocol::GameClientMessage{
                GameClientHello{MatchId{7U}, player, ticket}}));
    }

    void SendLobbyHello()
    {
        Send(dxa::protocol::EncodeClientMessage(
            dxa::protocol::ClientMessage{
                dxa::protocol::ClientHello{1U}}));
    }

    template <typename Message>
    [[nodiscard]] const Message* Latest() const
    {
        for (auto message = state_->messages.rbegin();
             message != state_->messages.rend();
             ++message)
        {
            if (const auto* value = std::get_if<Message>(&*message))
            {
                return value;
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool Closed() const
    {
        return state_->closedError.has_value();
    }

    [[nodiscard]] bool ProtocolError() const
    {
        return state_->protocolError;
    }

private:
    struct State
    {
        std::vector<GameServerMessage> messages;
        std::optional<boost::system::error_code> closedError;
        bool protocolError = false;
    };

    void Send(const dxa::protocol::EncodedMessage& message)
    {
        if (!transport_->Send(message))
        {
            throw std::runtime_error{"game probe send failed"};
        }
    }

    std::shared_ptr<State> state_;
    std::shared_ptr<AsioFramedConnection> transport_;
};

[[nodiscard]] dxa::game_server::GameServerConfig Config(
    const std::uint16_t controlPort)
{
    dxa::game_server::GameServerConfig config;
    config.options.lobbyControlPort = controlPort;
    config.options.gameTcpPort = 0U;
    config.options.gameUdpPort = 0U;
    config.authenticationTimeout = 20ms;
    config.controlReconnectDelay = 20ms;
    return config;
}

struct AdapterFixture
{
    AdapterFixture()
        : control{io},
          server{io, Config(control.Port())}
    {
        control.Start();
        server.Start();
    }

    ~AdapterFixture()
    {
        server.Stop();
        control.Stop();
        io.restart();
        while (io.poll_one() != 0U)
        {
        }
    }

    void WaitForRegistration()
    {
        RunUntil(io, [this] {
            return control.Latest<WorkerRegister>() != nullptr;
        });
    }

    void ReserveMatch()
    {
        WaitForRegistration();
        control.SendReservation();
        RunUntil(io, [this] {
            return control.Latest<dxa::protocol::ReserveMatchReady>()
                != nullptr;
        });
    }

    boost::asio::io_context io;
    FakeLobbyControl control;
    dxa::game_server::GameServer server;
};
} // namespace

TEST(GameServerAdapter, RegistersActualEphemeralGamePorts)
{
    AdapterFixture fixture;
    fixture.WaitForRegistration();

    const WorkerRegister* registration =
        fixture.control.Latest<WorkerRegister>();
    ASSERT_NE(nullptr, registration);
    EXPECT_EQ(WorkerId{1U}, registration->worker);
    EXPECT_EQ(fixture.server.GameTcpPort(), registration->gameTcpPort);
    EXPECT_EQ(fixture.server.GameUdpPort(), registration->gameUdpPort);
    EXPECT_NE(0U, registration->gameTcpPort);
    EXPECT_NE(0U, registration->gameUdpPort);
    EXPECT_FALSE(fixture.control.ProtocolError());
}

TEST(GameServerAdapter, ClosesGameTcpAfterAuthenticationTimeout)
{
    AdapterFixture fixture;
    fixture.WaitForRegistration();
    GameProbe client{fixture.io, fixture.server.GameTcpPort()};

    RunUntil(fixture.io, [&client] { return client.Closed(); });

    EXPECT_EQ(nullptr, client.Latest<GameServerWelcome>());
    EXPECT_FALSE(client.ProtocolError());
}

TEST(GameServerAdapter, RejectsSecondHelloAfterWelcomeAndFlushesError)
{
    AdapterFixture fixture;
    fixture.ReserveMatch();
    GameProbe client{fixture.io, fixture.server.GameTcpPort()};
    client.SendHello(PlayerId{1U}, Ticket(1U));
    RunUntil(fixture.io, [&client] {
        return client.Latest<GameServerWelcome>() != nullptr;
    });

    client.SendHello(PlayerId{1U}, Ticket(1U));
    RunUntil(fixture.io, [&client] {
        return client.Latest<GameServerErrorMessage>() != nullptr
            && client.Closed();
    });

    EXPECT_EQ(
        GameServerErrorCode::ProtocolViolation,
        client.Latest<GameServerErrorMessage>()->error);
    const auto traffic = fixture.server.Traffic();
    EXPECT_GT(traffic.tcpSentBytes, 0U);
    EXPECT_GT(traffic.tcpReceivedBytes, 0U);
    EXPECT_EQ(0U, traffic.udpSentBytes);
    EXPECT_EQ(0U, traffic.udpReceivedBytes);
}

TEST(GameServerAdapter, RejectsLobbyMessageOnGameChannel)
{
    AdapterFixture fixture;
    fixture.WaitForRegistration();
    GameProbe client{fixture.io, fixture.server.GameTcpPort()};

    client.SendLobbyHello();
    RunUntil(fixture.io, [&client] { return client.Closed(); });

    EXPECT_EQ(nullptr, client.Latest<GameServerWelcome>());
    EXPECT_EQ(nullptr, client.Latest<GameServerErrorMessage>());
    EXPECT_EQ(nullptr, client.Latest<dxa::protocol::GameMatchResult>());
    EXPECT_FALSE(client.ProtocolError());
}
