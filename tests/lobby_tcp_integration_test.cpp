#include "support/lobby_network_fixture.hpp"

#include <dxa/protocol/LobbyMessageCodec.hpp>
#include <dxa/protocol/TcpFrame.hpp>

#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <thread>
#include <variant>
#include <vector>

namespace
{
using boost::asio::ip::tcp;

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
