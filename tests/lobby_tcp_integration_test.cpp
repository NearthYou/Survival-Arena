#include "support/lobby_network_fixture.hpp"

#include <dxa/protocol/LobbyMessageCodec.hpp>
#include <dxa/protocol/TcpFrame.hpp>

#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <thread>
#include <variant>
#include <vector>

namespace
{
using boost::asio::ip::tcp;

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

[[nodiscard]] bool AllReady(const dxa::protocol::RoomSnapshot& snapshot)
{
    return !snapshot.members.empty()
        && std::all_of(
            snapshot.members.begin(),
            snapshot.members.end(),
            [](const auto& member) { return member.ready; });
}

[[nodiscard]] dxa::protocol::RoomId CreateAndReadyTwoPlayers(
    dxa::test::LobbyNetworkFixture& fixture,
    const std::shared_ptr<dxa::test::LobbyClientProbe>& host,
    const std::shared_ptr<dxa::test::LobbyClientProbe>& guest)
{
    fixture.ConnectAndWelcome(host);
    fixture.ConnectAndWelcome(guest);
    static_cast<void>(host->client->CreateRoom());
    fixture.RunUntil([&host] {
        return Latest<dxa::protocol::RoomSnapshot>(*host) != nullptr;
    });
    const dxa::protocol::RoomId room =
        Latest<dxa::protocol::RoomSnapshot>(*host)->room;

    static_cast<void>(guest->client->JoinRoom(room));
    fixture.RunUntil([&host, &guest] {
        const auto* hostRoom = Latest<dxa::protocol::RoomSnapshot>(*host);
        const auto* guestRoom = Latest<dxa::protocol::RoomSnapshot>(*guest);
        return hostRoom != nullptr
            && guestRoom != nullptr
            && hostRoom->members.size() == 2U
            && guestRoom->members.size() == 2U;
    });

    static_cast<void>(host->client->SetReady(true));
    static_cast<void>(guest->client->SetReady(true));
    fixture.RunUntil([&host, &guest] {
        const auto* hostRoom = Latest<dxa::protocol::RoomSnapshot>(*host);
        const auto* guestRoom = Latest<dxa::protocol::RoomSnapshot>(*guest);
        return hostRoom != nullptr
            && guestRoom != nullptr
            && AllReady(*hostRoom)
            && AllReady(*guestRoom);
    });
    return room;
}

[[nodiscard]] std::vector<std::byte> ReadExactWithDeadline(
    tcp::socket& socket,
    const std::size_t size)
{
    std::vector<std::byte> bytes(size);
    std::size_t read = 0U;
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{5};
    socket.non_blocking(true);
    while (read < bytes.size())
    {
        boost::system::error_code error;
        const std::size_t received = socket.read_some(
            boost::asio::buffer(bytes.data() + read, bytes.size() - read),
            error);
        if (!error)
        {
            read += received;
            continue;
        }
        if (error != boost::asio::error::would_block
            && error != boost::asio::error::try_again)
        {
            throw boost::system::system_error{error};
        }
        if (std::chrono::steady_clock::now() >= deadline)
        {
            throw std::runtime_error{"raw lobby read timed out"};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return bytes;
}

[[nodiscard]] dxa::protocol::ServerMessage ReadOneServerMessage(
    tcp::socket& socket)
{
    const auto headerBytes = ReadExactWithDeadline(
        socket,
        dxa::protocol::TcpFrameHeaderBytes);
    const auto decodedHeader = dxa::protocol::DecodeTcpFrameHeader(headerBytes);
    if (!decodedHeader.header.has_value())
    {
        throw std::runtime_error{"server returned an invalid frame header"};
    }
    const auto payload = ReadExactWithDeadline(
        socket,
        decodedHeader.header->payloadBytes);
    const auto decoded = dxa::protocol::DecodeServerMessage(
        decodedHeader.header->type,
        payload);
    if (!decoded.message.has_value())
    {
        throw std::runtime_error{"server returned an invalid lobby message"};
    }
    return *decoded.message;
}
} // namespace

TEST(LobbyTcpIntegration, RawHandshakeReturnsAssignedPlayer)
{
    dxa::test::RawLobbyServerFixture fixture;
    boost::asio::io_context clientIo;
    tcp::socket socket{clientIo};
    socket.connect({
        boost::asio::ip::make_address("127.0.0.1"),
        fixture.Port()});
    const auto hello = dxa::protocol::EncodeTcpFrame(
        dxa::protocol::EncodeClientMessage(
            dxa::protocol::ClientMessage{dxa::protocol::ClientHello{1U}}));
    boost::asio::write(socket, boost::asio::buffer(hello.data(), hello.size()));

    const dxa::protocol::ServerMessage response = ReadOneServerMessage(socket);
    const auto* welcome = std::get_if<dxa::protocol::ServerWelcome>(&response);
    ASSERT_NE(nullptr, welcome);
    EXPECT_EQ(1U, welcome->requestId);
    EXPECT_EQ(dxa::protocol::PlayerId{1U}, welcome->player);
}

TEST(LobbyTcpIntegration, TwoClientsCreateReadyStartAndReceiveDistinctTickets)
{
    dxa::test::LobbyNetworkFixture fixture{dxa::test::StaticEndpoint()};
    const auto host = fixture.AddClient();
    const auto guest = fixture.AddClient();
    static_cast<void>(CreateAndReadyTwoPlayers(fixture, host, guest));

    static_cast<void>(host->client->StartMatch());
    fixture.RunUntil([&host, &guest] {
        return Latest<dxa::protocol::MatchTicket>(*host) != nullptr
            && Latest<dxa::protocol::MatchTicket>(*guest) != nullptr;
    });

    const auto* hostTicket = Latest<dxa::protocol::MatchTicket>(*host);
    const auto* guestTicket = Latest<dxa::protocol::MatchTicket>(*guest);
    ASSERT_NE(nullptr, hostTicket);
    ASSERT_NE(nullptr, guestTicket);
    EXPECT_NE(hostTicket->ticket, guestTicket->ticket);
    EXPECT_EQ(dxa::protocol::RoomState::InMatch,
        Latest<dxa::protocol::RoomSnapshot>(*host)->state);
    EXPECT_EQ(dxa::protocol::RoomState::InMatch,
        Latest<dxa::protocol::RoomSnapshot>(*guest)->state);
}

TEST(LobbyTcpIntegration, WorkerFailureReturnsWaitingSnapshotsAndHostError)
{
    dxa::test::LobbyNetworkFixture fixture{std::nullopt};
    const auto host = fixture.AddClient();
    const auto guest = fixture.AddClient();
    static_cast<void>(CreateAndReadyTwoPlayers(fixture, host, guest));

    static_cast<void>(host->client->StartMatch());
    fixture.RunUntil([&host] {
        const auto* error = Latest<dxa::protocol::ErrorResponse>(*host);
        return error != nullptr
            && error->error == dxa::protocol::LobbyError::WorkerUnavailable;
    });

    EXPECT_EQ(dxa::protocol::RoomState::Waiting,
        Latest<dxa::protocol::RoomSnapshot>(*host)->state);
    EXPECT_EQ(dxa::protocol::RoomState::Waiting,
        Latest<dxa::protocol::RoomSnapshot>(*guest)->state);
    EXPECT_TRUE(AllReady(*Latest<dxa::protocol::RoomSnapshot>(*host)));
    EXPECT_TRUE(AllReady(*Latest<dxa::protocol::RoomSnapshot>(*guest)));
}
