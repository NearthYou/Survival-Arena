#pragma once

#include <dxa/protocol/TcpFrame.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <array>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

namespace dxa::protocol
{
struct RawFrame
{
    MessageType type = MessageType::ClientHello;
    std::vector<std::byte> payload;
};

enum class TrafficDirection
{
    Sent,
    Received
};

using ByteObserver = std::function<void(TrafficDirection, std::size_t)>;

class AsioFramedConnection final
    : public std::enable_shared_from_this<AsioFramedConnection>
{
public:
    using FrameHandler = std::function<void(RawFrame)>;
    using CloseHandler = std::function<void(boost::system::error_code)>;

    [[nodiscard]] static std::shared_ptr<AsioFramedConnection> Create(
        boost::asio::ip::tcp::socket socket,
        FrameHandler onFrame,
        CloseHandler onClose,
        ByteObserver onBytes = {});

    void Start();
    [[nodiscard]] bool Send(const EncodedMessage& message);
    void Close();
    void CloseAfterFlush();
    [[nodiscard]] boost::asio::ip::tcp::socket& Socket() noexcept;

private:
    AsioFramedConnection(
        boost::asio::ip::tcp::socket socket,
        FrameHandler onFrame,
        CloseHandler onClose,
        ByteObserver onBytes);

    void ReadHeader();
    void ReadPayload(const TcpFrameHeader& header);
    void DeliverFrame(MessageType type);
    void WriteNext();
    void Observe(TrafficDirection direction, std::size_t bytes) noexcept;
    void FinishClose(boost::system::error_code error);

    boost::asio::ip::tcp::socket socket_;
    FrameHandler onFrame_;
    CloseHandler onClose_;
    ByteObserver onBytes_;
    std::array<std::byte, TcpFrameHeaderBytes> headerBytes_{};
    std::vector<std::byte> payload_;
    std::deque<std::vector<std::byte>> writeQueue_;
    std::size_t pendingWriteBytes_ = 0U;
    bool started_ = false;
    bool closed_ = false;
    bool writeInProgress_ = false;
    bool closeAfterFlush_ = false;
};
} // namespace dxa::protocol
