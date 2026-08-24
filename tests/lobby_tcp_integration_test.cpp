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

void WaitForMemberCount(
    dxa::test::LobbyNetworkFixture& fixture,
    const std::shared_ptr<dxa::test::LobbyClientProbe>& observer,
    const std::size_t count)
{
    fixture.RunUntil([&observer, count] {
        const auto* room = Latest<dxa::protocol::RoomSnapshot>(*observer);
        return room != nullptr && room->members.size() == count;
    });
}

void WaitForPeerClose(tcp::socket& socket)
{
    socket.non_blocking(true);
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{5};
    std::array<std::byte, 1U> byte{};
    for (;;)
    {
        boost::system::error_code error;
        static_cast<void>(socket.read_some(boost::asio::buffer(byte), error));
        if (error == boost::asio::error::eof
            || error == boost::asio::error::connection_reset
            || error == boost::asio::error::operation_aborted)
        {
            return;
        }
        if (error && error != boost::asio::error::would_block
            && error != boost::asio::error::try_again)
        {
            throw boost::system::system_error{error};
        }
        if (std::chrono::steady_clock::now() >= deadline)
        {
            throw std::runtime_error{"raw lobby close timed out"};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
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

TEST(LobbyTcpIntegration, WorkerFailureReturnsWaitingSnapshotsAndHostError)
{
    dxa::test::LobbyNetworkFixture fixture;
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

TEST(LobbyTcpIntegration, AcceptsTwentyFourAndRejectsTwentyFifthConnectionFromRoom)
{
    dxa::test::LobbyNetworkFixture fixture;
    const auto clients = fixture.AddWelcomedClients(25U);
    static_cast<void>(clients[0]->client->CreateRoom());
    WaitForMemberCount(fixture, clients[0], 1U);
    const dxa::protocol::RoomId room =
        Latest<dxa::protocol::RoomSnapshot>(*clients[0])->room;

    for (std::size_t index = 1U; index < 24U; ++index)
    {
        static_cast<void>(clients[index]->client->JoinRoom(room));
        WaitForMemberCount(fixture, clients[0], index + 1U);
    }
    static_cast<void>(clients[24]->client->JoinRoom(room));
    fixture.RunUntil([&clients] {
        const auto* error =
            Latest<dxa::protocol::ErrorResponse>(*clients[24]);
        return error != nullptr
            && error->error == dxa::protocol::LobbyError::RoomFull;
    });

    ASSERT_NE(nullptr, Latest<dxa::protocol::RoomSnapshot>(*clients[0]));
    EXPECT_EQ(
        24U,
        Latest<dxa::protocol::RoomSnapshot>(*clients[0])->members.size());
}

TEST(LobbyTcpIntegration, ClosingHostSocketBroadcastsSuccessor)
{
    dxa::test::LobbyNetworkFixture fixture;
    const auto clients = fixture.AddWelcomedClients(3U);
    static_cast<void>(clients[0]->client->CreateRoom());
    WaitForMemberCount(fixture, clients[0], 1U);
    const dxa::protocol::RoomId room =
        Latest<dxa::protocol::RoomSnapshot>(*clients[0])->room;
    static_cast<void>(clients[1]->client->JoinRoom(room));
    WaitForMemberCount(fixture, clients[0], 2U);
    static_cast<void>(clients[2]->client->JoinRoom(room));
    WaitForMemberCount(fixture, clients[0], 3U);

    clients[0]->client->Close();
    fixture.RunUntil([&clients] {
        const auto* snapshot =
            Latest<dxa::protocol::RoomSnapshot>(*clients[1]);
        return snapshot != nullptr
            && snapshot->host == dxa::protocol::PlayerId{2U}
            && snapshot->members.size() == 2U;
    });

    EXPECT_EQ(
        dxa::protocol::PlayerId{2U},
        Latest<dxa::protocol::RoomSnapshot>(*clients[1])->host);
}

TEST(LobbyTcpIntegration, OversizedHeaderClosesWithoutCreatingRoom)
{
    dxa::test::LobbyNetworkFixture fixture;
    boost::asio::io_context rawIo;
    tcp::socket raw{rawIo};
    raw.connect({
        boost::asio::ip::make_address("127.0.0.1"),
        fixture.Port()});
    auto oversizedHeader = dxa::protocol::EncodeTcpFrame(
        dxa::protocol::EncodedMessage{
            dxa::protocol::MessageType::CreateRoomRequest,
            {}});
    const std::uint32_t oversized = static_cast<std::uint32_t>(
        dxa::protocol::MaxTcpPayloadBytes + 1U);
    oversizedHeader[8] = static_cast<std::byte>(oversized & 0xFFU);
    oversizedHeader[9] = static_cast<std::byte>((oversized >> 8U) & 0xFFU);
    oversizedHeader[10] = static_cast<std::byte>((oversized >> 16U) & 0xFFU);
    oversizedHeader[11] = static_cast<std::byte>((oversized >> 24U) & 0xFFU);
    boost::asio::write(
        raw,
        boost::asio::buffer(oversizedHeader.data(), oversizedHeader.size()));
    WaitForPeerClose(raw);

    const auto observer = fixture.AddClient();
    fixture.ConnectAndWelcome(observer);
    static_cast<void>(observer->client->ListRooms());
    fixture.RunUntil([&observer] {
        return Latest<dxa::protocol::RoomListResponse>(*observer) != nullptr;
    });
    EXPECT_TRUE(
        Latest<dxa::protocol::RoomListResponse>(*observer)->rooms.empty());
}
