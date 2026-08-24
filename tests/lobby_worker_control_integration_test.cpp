#include "support/lobby_network_fixture.hpp"

#include <dxa/protocol/AsioFramedConnection.hpp>
#include <dxa/protocol/WorkerControlMessageCodec.hpp>

#include <gtest/gtest.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using namespace std::chrono_literals;
using dxa::protocol::CancelMatchReservation;
using dxa::protocol::ErrorResponse;
using dxa::protocol::LobbyError;
using dxa::protocol::LobbyToWorkerMessage;
using dxa::protocol::MatchCompletionReason;
using dxa::protocol::MatchFinished;
using dxa::protocol::MatchId;
using dxa::protocol::MatchReservationCancelled;
using dxa::protocol::MatchTicket;
using dxa::protocol::ReserveMatch;
using dxa::protocol::ReserveMatchReady;
using dxa::protocol::ReserveMatchRejected;
using dxa::protocol::RoomId;
using dxa::protocol::RoomListResponse;
using dxa::protocol::RoomSnapshot;
using dxa::protocol::RoomState;
using dxa::protocol::WorkerId;
using dxa::protocol::WorkerRegister;
using dxa::protocol::WorkerRegistered;
using dxa::protocol::WorkerReservationReject;
using dxa::protocol::WorkerToLobbyMessage;

template <typename Message>
[[nodiscard]] const Message* Latest(
    const dxa::test::LobbyClientProbe& probe)
{
    for (auto message = probe.messages.rbegin();
         message != probe.messages.rend();
         ++message)
    {
        if (const auto* value = std::get_if<Message>(&*message))
        {
            return value;
        }
    }
    return nullptr;
}

[[nodiscard]] bool AllReady(const RoomSnapshot& snapshot)
{
    return snapshot.members.size() >= 2U
        && std::all_of(
            snapshot.members.begin(),
            snapshot.members.end(),
            [](const auto& member) { return member.ready; });
}

struct ReadyNetworkRoom
{
    std::shared_ptr<dxa::test::LobbyClientProbe> host;
    std::shared_ptr<dxa::test::LobbyClientProbe> guest;
    RoomId room;
};

[[nodiscard]] ReadyNetworkRoom CreateReadyTwoPlayerRoom(
    dxa::test::LobbyNetworkFixture& fixture)
{
    const auto host = fixture.AddClient();
    const auto guest = fixture.AddClient();
    fixture.ConnectAndWelcome(host);
    fixture.ConnectAndWelcome(guest);
    static_cast<void>(host->client->CreateRoom());
    fixture.RunUntil([&host] {
        return Latest<RoomSnapshot>(*host) != nullptr;
    });
    const RoomId room = Latest<RoomSnapshot>(*host)->room;

    static_cast<void>(guest->client->JoinRoom(room));
    fixture.RunUntil([&host, &guest] {
        const auto* hostRoom = Latest<RoomSnapshot>(*host);
        const auto* guestRoom = Latest<RoomSnapshot>(*guest);
        return hostRoom != nullptr
            && guestRoom != nullptr
            && hostRoom->members.size() == 2U
            && guestRoom->members.size() == 2U;
    });
    static_cast<void>(host->client->SetReady(true));
    static_cast<void>(guest->client->SetReady(true));
    fixture.RunUntil([&host, &guest] {
        const auto* hostRoom = Latest<RoomSnapshot>(*host);
        const auto* guestRoom = Latest<RoomSnapshot>(*guest);
        return hostRoom != nullptr
            && guestRoom != nullptr
            && AllReady(*hostRoom)
            && AllReady(*guestRoom);
    });
    return {host, guest, room};
}

class WorkerProbe
{
public:
    WorkerProbe(
        boost::asio::io_context& io,
        const std::uint16_t port)
        : state_{std::make_shared<State>()}
    {
        boost::asio::ip::tcp::socket socket{io};
        socket.connect({
            boost::asio::ip::make_address("127.0.0.1"),
            port});
        const std::weak_ptr<State> weak = state_;
        transport_ = dxa::protocol::AsioFramedConnection::Create(
            std::move(socket),
            [weak](dxa::protocol::RawFrame frame) {
                if (const auto state = weak.lock())
                {
                    const auto decoded =
                        dxa::protocol::DecodeLobbyToWorkerMessage(
                            frame.type,
                            frame.payload);
                    if (!decoded.message.has_value())
                    {
                        state->protocolError = true;
                        return;
                    }
                    state->messages.push_back(*decoded.message);
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

    WorkerProbe(const WorkerProbe&) = delete;
    WorkerProbe& operator=(const WorkerProbe&) = delete;

    ~WorkerProbe()
    {
        transport_->Close();
    }

    void Register(
        const WorkerId worker = WorkerId{1U},
        const std::uint16_t tcpPort = 7100U,
        const std::uint16_t udpPort = 7101U)
    {
        Send(WorkerToLobbyMessage{WorkerRegister{
            worker,
            "127.0.0.1",
            tcpPort,
            udpPort,
            1U}});
    }

    void SendReady(const ReserveMatch& reservation)
    {
        Send(WorkerToLobbyMessage{ReserveMatchReady{
            reservation.reservation,
            reservation.match}});
    }

    void SendRejected(const ReserveMatch& reservation)
    {
        Send(WorkerToLobbyMessage{ReserveMatchRejected{
            reservation.reservation,
            reservation.match,
            WorkerReservationReject::SimulationInitializationFailed}});
    }

    void SendCancelled(const CancelMatchReservation& cancellation)
    {
        Send(WorkerToLobbyMessage{MatchReservationCancelled{
            cancellation.reservation,
            cancellation.match}});
    }

    void SendFinished(const MatchId match)
    {
        Send(WorkerToLobbyMessage{MatchFinished{
            match,
            dxa::protocol::EntityId{0U},
            true,
            MatchCompletionReason::LastSurvivor,
            900U}});
    }

    void SendReadyBeforeRegistration()
    {
        Send(WorkerToLobbyMessage{ReserveMatchReady{
            dxa::protocol::ReservationId{1U},
            MatchId{1U}}});
    }

    void Close()
    {
        transport_->Close();
    }

    [[nodiscard]] bool Registered() const
    {
        return LatestMessage<WorkerRegistered>() != nullptr;
    }

    [[nodiscard]] bool Closed() const
    {
        return state_->closedError.has_value();
    }

    [[nodiscard]] bool ProtocolError() const
    {
        return state_->protocolError;
    }

    [[nodiscard]] std::size_t ReservationCount() const
    {
        return static_cast<std::size_t>(std::count_if(
            state_->messages.begin(),
            state_->messages.end(),
            [](const auto& message) {
                return std::holds_alternative<ReserveMatch>(message);
            }));
    }

    template <typename Message>
    [[nodiscard]] const Message* LatestMessage() const
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

private:
    struct State
    {
        std::vector<LobbyToWorkerMessage> messages;
        std::optional<boost::system::error_code> closedError;
        bool protocolError = false;
    };

    void Send(WorkerToLobbyMessage message)
    {
        const auto encoded = dxa::protocol::EncodeWorkerToLobbyMessage(
            message);
        if (!transport_->Send(encoded))
        {
            throw std::runtime_error{"worker probe send failed"};
        }
    }

    std::shared_ptr<State> state_;
    std::shared_ptr<dxa::protocol::AsioFramedConnection> transport_;
};

void RegisterWorker(
    dxa::test::LobbyNetworkFixture& fixture,
    WorkerProbe& worker)
{
    worker.Register();
    fixture.RunUntil([&worker] { return worker.Registered(); });
}

void WaitForWorkerUnavailable(
    dxa::test::LobbyNetworkFixture& fixture,
    const std::shared_ptr<dxa::test::LobbyClientProbe>& host)
{
    fixture.RunUntil([&host] {
        const auto* error = Latest<ErrorResponse>(*host);
        return error != nullptr
            && error->error == LobbyError::WorkerUnavailable;
    });
}
} // namespace

TEST(LobbyWorkerControl, RequiresRegistrationAsFirstFrame)
{
    dxa::test::LobbyNetworkFixture fixture;
    WorkerProbe worker{fixture.Io(), fixture.WorkerPort()};

    worker.SendReadyBeforeRegistration();
    fixture.RunUntil([&worker] { return worker.Closed(); });

    EXPECT_FALSE(worker.Registered());
    EXPECT_FALSE(worker.ProtocolError());
}

TEST(LobbyWorkerControl, DoesNotPublishTicketBeforeRealWorkerReady)
{
    dxa::test::LobbyNetworkFixture fixture;
    WorkerProbe worker{fixture.Io(), fixture.WorkerPort()};
    RegisterWorker(fixture, worker);
    const ReadyNetworkRoom room = CreateReadyTwoPlayerRoom(fixture);

    static_cast<void>(room.host->client->StartMatch());
    fixture.RunUntil([&worker] { return worker.ReservationCount() == 1U; });
    ASSERT_EQ(nullptr, Latest<MatchTicket>(*room.host));
    ASSERT_EQ(nullptr, Latest<MatchTicket>(*room.guest));

    const ReserveMatch reservation =
        *worker.LatestMessage<ReserveMatch>();
    worker.SendReady(reservation);
    fixture.RunUntil([&room] {
        return Latest<MatchTicket>(*room.host) != nullptr
            && Latest<MatchTicket>(*room.guest) != nullptr;
    });

    const MatchTicket* hostTicket = Latest<MatchTicket>(*room.host);
    const MatchTicket* guestTicket = Latest<MatchTicket>(*room.guest);
    ASSERT_NE(nullptr, hostTicket);
    ASSERT_NE(nullptr, guestTicket);
    EXPECT_TRUE(hostTicket->ticket != guestTicket->ticket);
    EXPECT_EQ("127.0.0.1", hostTicket->host);
    EXPECT_EQ(7100U, hostTicket->tcpPort);
    EXPECT_EQ(7101U, hostTicket->udpPort);
    EXPECT_EQ(RoomState::InMatch, Latest<RoomSnapshot>(*room.host)->state);
}

TEST(LobbyWorkerControl, RejectionRollsRoomBackAndKeepsWorkerReusable)
{
    dxa::test::LobbyNetworkFixture fixture;
    WorkerProbe worker{fixture.Io(), fixture.WorkerPort()};
    RegisterWorker(fixture, worker);
    const ReadyNetworkRoom room = CreateReadyTwoPlayerRoom(fixture);
    static_cast<void>(room.host->client->StartMatch());
    fixture.RunUntil([&worker] { return worker.ReservationCount() == 1U; });

    worker.SendRejected(*worker.LatestMessage<ReserveMatch>());
    WaitForWorkerUnavailable(fixture, room.host);

    ASSERT_NE(nullptr, Latest<RoomSnapshot>(*room.host));
    EXPECT_EQ(RoomState::Waiting, Latest<RoomSnapshot>(*room.host)->state);
    EXPECT_TRUE(AllReady(*Latest<RoomSnapshot>(*room.host)));
    static_cast<void>(room.host->client->StartMatch());
    fixture.RunUntil([&worker] { return worker.ReservationCount() == 2U; });
}

TEST(LobbyWorkerControl, ReservationTimeoutUsesConfiguredTestDurationAndClosesWorker)
{
    const dxa::lobby::WorkerControlServerConfig defaults;
    EXPECT_EQ(2s, defaults.reservationTimeout);
    dxa::lobby::WorkerControlServerConfig testConfig;
    testConfig.reservationTimeout = 20ms;
    dxa::test::LobbyNetworkFixture fixture{testConfig};
    WorkerProbe worker{fixture.Io(), fixture.WorkerPort()};
    RegisterWorker(fixture, worker);
    const ReadyNetworkRoom room = CreateReadyTwoPlayerRoom(fixture);

    static_cast<void>(room.host->client->StartMatch());
    fixture.RunUntil([&worker] { return worker.ReservationCount() == 1U; });
    WaitForWorkerUnavailable(fixture, room.host);
    fixture.RunUntil([&worker] { return worker.Closed(); });

    EXPECT_EQ(RoomState::Waiting, Latest<RoomSnapshot>(*room.host)->state);
}

TEST(LobbyWorkerControl, StartingDisconnectCancelsReservationAndAckReusesWorker)
{
    dxa::test::LobbyNetworkFixture fixture;
    WorkerProbe worker{fixture.Io(), fixture.WorkerPort()};
    RegisterWorker(fixture, worker);
    const ReadyNetworkRoom room = CreateReadyTwoPlayerRoom(fixture);
    static_cast<void>(room.host->client->StartMatch());
    fixture.RunUntil([&worker] { return worker.ReservationCount() == 1U; });

    room.host->client->Close();
    fixture.RunUntil([&worker, &room] {
        const auto* cancellation =
            worker.LatestMessage<CancelMatchReservation>();
        const auto* snapshot = Latest<RoomSnapshot>(*room.guest);
        return cancellation != nullptr
            && snapshot != nullptr
            && snapshot->state == RoomState::Waiting
            && snapshot->members.size() == 1U;
    });
    worker.SendCancelled(*worker.LatestMessage<CancelMatchReservation>());

    const auto newcomer = fixture.AddClient();
    fixture.ConnectAndWelcome(newcomer);
    static_cast<void>(newcomer->client->JoinRoom(room.room));
    fixture.RunUntil([&room, &newcomer] {
        const auto* guestRoom = Latest<RoomSnapshot>(*room.guest);
        const auto* newcomerRoom = Latest<RoomSnapshot>(*newcomer);
        return guestRoom != nullptr
            && newcomerRoom != nullptr
            && guestRoom->members.size() == 2U
            && newcomerRoom->members.size() == 2U;
    });
    static_cast<void>(newcomer->client->SetReady(true));
    fixture.RunUntil([&room] {
        const auto* snapshot = Latest<RoomSnapshot>(*room.guest);
        return snapshot != nullptr && AllReady(*snapshot);
    });
    static_cast<void>(room.guest->client->StartMatch());
    fixture.RunUntil([&worker] { return worker.ReservationCount() == 2U; });
}

TEST(LobbyWorkerControl, ReservedControlCloseRollsBackPendingStart)
{
    dxa::test::LobbyNetworkFixture fixture;
    WorkerProbe worker{fixture.Io(), fixture.WorkerPort()};
    RegisterWorker(fixture, worker);
    const ReadyNetworkRoom room = CreateReadyTwoPlayerRoom(fixture);
    static_cast<void>(room.host->client->StartMatch());
    fixture.RunUntil([&worker] { return worker.ReservationCount() == 1U; });

    worker.Close();
    WaitForWorkerUnavailable(fixture, room.host);

    EXPECT_EQ(RoomState::Waiting, Latest<RoomSnapshot>(*room.host)->state);
    EXPECT_TRUE(AllReady(*Latest<RoomSnapshot>(*room.host)));
}

TEST(LobbyWorkerControl, ActiveControlClosePublishesMatchUnavailableAndDeletesRoom)
{
    dxa::test::LobbyNetworkFixture fixture;
    WorkerProbe worker{fixture.Io(), fixture.WorkerPort()};
    RegisterWorker(fixture, worker);
    const ReadyNetworkRoom room = CreateReadyTwoPlayerRoom(fixture);
    static_cast<void>(room.host->client->StartMatch());
    fixture.RunUntil([&worker] { return worker.ReservationCount() == 1U; });
    worker.SendReady(*worker.LatestMessage<ReserveMatch>());
    fixture.RunUntil([&room] {
        return Latest<MatchTicket>(*room.host) != nullptr
            && Latest<MatchTicket>(*room.guest) != nullptr;
    });

    worker.Close();
    fixture.RunUntil([&room] {
        const auto* hostError = Latest<ErrorResponse>(*room.host);
        const auto* guestError = Latest<ErrorResponse>(*room.guest);
        const auto* hostRooms = Latest<RoomListResponse>(*room.host);
        const auto* guestRooms = Latest<RoomListResponse>(*room.guest);
        return hostError != nullptr
            && guestError != nullptr
            && hostError->error == LobbyError::MatchUnavailable
            && guestError->error == LobbyError::MatchUnavailable
            && hostRooms != nullptr
            && guestRooms != nullptr
            && hostRooms->rooms.empty()
            && guestRooms->rooms.empty();
    });

    EXPECT_EQ(LobbyError::MatchUnavailable,
              Latest<ErrorResponse>(*room.guest)->error);
    EXPECT_TRUE(Latest<RoomListResponse>(*room.guest)->rooms.empty());
}

TEST(LobbyWorkerControl, MatchFinishedDeletesRoomWithoutPublishingFailure)
{
    dxa::test::LobbyNetworkFixture fixture;
    WorkerProbe worker{fixture.Io(), fixture.WorkerPort()};
    RegisterWorker(fixture, worker);
    const ReadyNetworkRoom room = CreateReadyTwoPlayerRoom(fixture);
    static_cast<void>(room.host->client->StartMatch());
    fixture.RunUntil([&worker] { return worker.ReservationCount() == 1U; });
    const ReserveMatch reservation =
        *worker.LatestMessage<ReserveMatch>();
    worker.SendReady(reservation);
    fixture.RunUntil([&room] {
        return Latest<MatchTicket>(*room.host) != nullptr
            && Latest<MatchTicket>(*room.guest) != nullptr;
    });

    worker.SendFinished(reservation.match);
    fixture.RunUntil([&room] {
        const auto* hostRooms = Latest<RoomListResponse>(*room.host);
        const auto* guestRooms = Latest<RoomListResponse>(*room.guest);
        return hostRooms != nullptr
            && guestRooms != nullptr
            && hostRooms->rooms.empty()
            && guestRooms->rooms.empty();
    });

    EXPECT_EQ(nullptr, Latest<ErrorResponse>(*room.host));
    EXPECT_EQ(nullptr, Latest<ErrorResponse>(*room.guest));
}
