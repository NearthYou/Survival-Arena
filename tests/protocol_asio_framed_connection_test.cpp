#include <dxa/protocol/AsioFramedConnection.hpp>

#include <dxa/protocol/TcpFrame.hpp>

#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
using boost::asio::ip::tcp;
using dxa::protocol::AsioFramedConnection;
using dxa::protocol::EncodedMessage;
using dxa::protocol::MessageType;
using dxa::protocol::RawFrame;

struct AsioSocketPair
{
    AsioSocketPair()
        : client{io},
          server{io}
    {
        tcp::acceptor acceptor{
            io,
            tcp::endpoint{boost::asio::ip::make_address("127.0.0.1"), 0U}};
        client.connect(acceptor.local_endpoint());
        acceptor.accept(server);
    }

    boost::asio::io_context io;
    tcp::socket client;
    tcp::socket server;
};

[[nodiscard]] EncodedMessage Message(
    const MessageType type,
    const std::initializer_list<std::uint8_t> payload)
{
    EncodedMessage message;
    message.type = type;
    message.payload.reserve(payload.size());
    for (const std::uint8_t value : payload)
    {
        message.payload.push_back(static_cast<std::byte>(value));
    }
    return message;
}

void RunUntil(
    boost::asio::io_context& io,
    const std::function<bool()>& condition)
{
    bool timedOut = false;
    boost::asio::steady_timer timer{io};
    timer.expires_after(std::chrono::seconds{5});
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
        throw std::runtime_error{"Asio test timed out"};
    }
}

void PumpReady(boost::asio::io_context& io)
{
    io.restart();
    while (io.poll_one() != 0U)
    {
    }
}

void WriteFragment(
    tcp::socket& socket,
    const std::span<const std::byte> bytes)
{
    boost::asio::write(socket, boost::asio::buffer(bytes.data(), bytes.size()));
}

[[nodiscard]] std::vector<std::byte> Concatenate(
    const std::vector<std::byte>& first,
    const std::vector<std::byte>& second)
{
    std::vector<std::byte> bytes;
    bytes.reserve(first.size() + second.size());
    bytes.insert(bytes.end(), first.begin(), first.end());
    bytes.insert(bytes.end(), second.begin(), second.end());
    return bytes;
}
} // namespace

TEST(AsioFramedConnection, ReadsFragmentedFrameAndPreservesWriteOrder)
{
    AsioSocketPair pair;
    std::vector<RawFrame> received;
    std::size_t closeCount = 0U;
    auto connection = AsioFramedConnection::Create(
        std::move(pair.server),
        [&received](RawFrame frame) { received.push_back(std::move(frame)); },
        [&closeCount](const boost::system::error_code) { ++closeCount; });
    connection->Start();

    const auto incoming = dxa::protocol::EncodeTcpFrame(
        Message(MessageType::ClientHello, {1U, 2U, 3U, 4U}));
    WriteFragment(pair.client, std::span{incoming}.subspan(0U, 3U));
    PumpReady(pair.io);
    EXPECT_TRUE(received.empty());
    WriteFragment(pair.client, std::span{incoming}.subspan(3U, 9U));
    PumpReady(pair.io);
    EXPECT_TRUE(received.empty());
    WriteFragment(pair.client, std::span{incoming}.subspan(12U));
    RunUntil(pair.io, [&received] { return received.size() == 1U; });

    ASSERT_EQ(1U, received.size());
    EXPECT_EQ(MessageType::ClientHello, received.front().type);
    EXPECT_EQ(
        Message(MessageType::ClientHello, {1U, 2U, 3U, 4U}).payload,
        received.front().payload);

    const EncodedMessage first = Message(MessageType::RoomListResponse, {5U});
    const EncodedMessage second = Message(MessageType::RoomSnapshot, {6U, 7U});
    const auto firstFrame = dxa::protocol::EncodeTcpFrame(first);
    const auto secondFrame = dxa::protocol::EncodeTcpFrame(second);
    const auto expected = Concatenate(firstFrame, secondFrame);
    std::vector<std::byte> written(expected.size());
    bool writeRead = false;
    boost::asio::async_read(
        pair.client,
        boost::asio::buffer(written.data(), written.size()),
        [&writeRead](const boost::system::error_code error, const std::size_t) {
            writeRead = !error;
        });

    EXPECT_TRUE(connection->Send(first));
    EXPECT_TRUE(connection->Send(second));
    RunUntil(pair.io, [&writeRead] { return writeRead; });
    EXPECT_EQ(expected, written);
    EXPECT_EQ(0U, closeCount);

    connection->Close();
    EXPECT_EQ(1U, closeCount);
}

TEST(AsioFramedConnection, RejectsOversizedHeaderBeforeReadingPayload)
{
    AsioSocketPair pair;
    std::vector<RawFrame> received;
    std::size_t closeCount = 0U;
    auto connection = AsioFramedConnection::Create(
        std::move(pair.server),
        [&received](RawFrame frame) { received.push_back(std::move(frame)); },
        [&closeCount](const boost::system::error_code) { ++closeCount; });
    connection->Start();

    auto header = dxa::protocol::EncodeTcpFrame(
        Message(MessageType::ClientHello, {}));
    const std::uint32_t oversized = static_cast<std::uint32_t>(
        dxa::protocol::MaxTcpPayloadBytes + 1U);
    header[8] = static_cast<std::byte>(oversized & 0xFFU);
    header[9] = static_cast<std::byte>((oversized >> 8U) & 0xFFU);
    header[10] = static_cast<std::byte>((oversized >> 16U) & 0xFFU);
    header[11] = static_cast<std::byte>((oversized >> 24U) & 0xFFU);
    WriteFragment(pair.client, header);

    RunUntil(pair.io, [&closeCount] { return closeCount == 1U; });
    EXPECT_TRUE(received.empty());
    EXPECT_FALSE(connection->Socket().is_open());
}

TEST(AsioFramedConnection, ClosesTruncatedSocketExactlyOnce)
{
    AsioSocketPair pair;
    std::size_t closeCount = 0U;
    auto connection = AsioFramedConnection::Create(
        std::move(pair.server),
        [](RawFrame) {},
        [&closeCount](const boost::system::error_code) { ++closeCount; });
    connection->Start();

    const auto frame = dxa::protocol::EncodeTcpFrame(
        Message(MessageType::ClientHello, {1U, 2U}));
    WriteFragment(pair.client, std::span{frame}.first(3U));
    pair.client.close();

    RunUntil(pair.io, [&closeCount] { return closeCount == 1U; });
    PumpReady(pair.io);
    connection->Close();
    EXPECT_EQ(1U, closeCount);
}

TEST(AsioFramedConnection, ManualCloseInvokesCallbackOnlyOnce)
{
    AsioSocketPair pair;
    std::size_t closeCount = 0U;
    auto connection = AsioFramedConnection::Create(
        std::move(pair.server),
        [](RawFrame) {},
        [&closeCount](const boost::system::error_code) { ++closeCount; });
    connection->Start();

    connection->Close();
    connection->Close();
    PumpReady(pair.io);

    EXPECT_EQ(1U, closeCount);
}

TEST(AsioFramedConnection, KeepsItselfAliveDuringReentrantCloseCallback)
{
    AsioSocketPair pair;
    std::shared_ptr<AsioFramedConnection> connection;
    std::weak_ptr<AsioFramedConnection> weak;
    bool aliveDuringCallback = false;
    connection = AsioFramedConnection::Create(
        std::move(pair.server),
        [](RawFrame) {},
        [&connection, &weak, &aliveDuringCallback](
            const boost::system::error_code) {
            connection.reset();
            aliveDuringCallback = !weak.expired();
        });
    weak = connection;
    AsioFramedConnection* const raw = connection.get();

    raw->Close();

    EXPECT_TRUE(aliveDuringCallback);
    EXPECT_TRUE(weak.expired());
}

TEST(AsioFramedConnection, ClosesWhenPendingWritesExceedLimit)
{
    AsioSocketPair pair;
    std::size_t closeCount = 0U;
    auto connection = AsioFramedConnection::Create(
        std::move(pair.server),
        [](RawFrame) {},
        [&closeCount](const boost::system::error_code) { ++closeCount; });
    connection->Start();

    EncodedMessage maximum;
    maximum.type = MessageType::RoomSnapshot;
    maximum.payload.resize(dxa::protocol::MaxTcpPayloadBytes);
    EXPECT_TRUE(connection->Send(maximum));
    EXPECT_TRUE(connection->Send(maximum));
    EXPECT_TRUE(connection->Send(maximum));
    EXPECT_TRUE(connection->Send(maximum));
    EXPECT_FALSE(connection->Send(maximum));
    EXPECT_EQ(1U, closeCount);
    EXPECT_FALSE(connection->Socket().is_open());

    PumpReady(pair.io);
    EXPECT_EQ(1U, closeCount);
}
