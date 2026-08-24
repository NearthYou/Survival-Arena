#include <dxa/protocol/AsioFramedConnection.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <exception>
#include <utility>

namespace dxa::protocol
{
namespace
{
[[nodiscard]] boost::system::error_code HeaderError(
    const FrameHeaderError error) noexcept
{
    if (error == FrameHeaderError::FrameTooLarge)
    {
        return boost::asio::error::message_size;
    }
    return boost::asio::error::invalid_argument;
}
} // namespace

std::shared_ptr<AsioFramedConnection> AsioFramedConnection::Create(
    boost::asio::ip::tcp::socket socket,
    FrameHandler onFrame,
    CloseHandler onClose)
{
    return std::shared_ptr<AsioFramedConnection>{
        new AsioFramedConnection{
            std::move(socket),
            std::move(onFrame),
            std::move(onClose)}};
}

AsioFramedConnection::AsioFramedConnection(
    boost::asio::ip::tcp::socket socket,
    FrameHandler onFrame,
    CloseHandler onClose)
    : socket_{std::move(socket)},
      onFrame_{std::move(onFrame)},
      onClose_{std::move(onClose)}
{
}

void AsioFramedConnection::Start()
{
    if (started_ || closed_)
    {
        return;
    }
    started_ = true;
    ReadHeader();
}

bool AsioFramedConnection::Send(const EncodedMessage& message)
{
    const auto keepAlive = shared_from_this();
    static_cast<void>(keepAlive);
    if (closed_ || closeAfterFlush_)
    {
        return false;
    }

    std::vector<std::byte> frame;
    try
    {
        frame = EncodeTcpFrame(message);
    }
    catch (const std::exception&)
    {
        FinishClose(boost::asio::error::message_size);
        return false;
    }

    if (frame.size() > MaxPendingWriteBytes - pendingWriteBytes_)
    {
        FinishClose(boost::asio::error::no_buffer_space);
        return false;
    }

    pendingWriteBytes_ += frame.size();
    writeQueue_.push_back(std::move(frame));
    if (!writeInProgress_)
    {
        WriteNext();
    }
    return true;
}

void AsioFramedConnection::Close()
{
    const auto keepAlive = shared_from_this();
    static_cast<void>(keepAlive);
    FinishClose(boost::asio::error::operation_aborted);
}

void AsioFramedConnection::CloseAfterFlush()
{
    const auto keepAlive = shared_from_this();
    static_cast<void>(keepAlive);
    if (closed_ || closeAfterFlush_)
    {
        return;
    }
    closeAfterFlush_ = true;
    if (!writeInProgress_ && writeQueue_.empty())
    {
        FinishClose(boost::asio::error::operation_aborted);
    }
}

boost::asio::ip::tcp::socket& AsioFramedConnection::Socket() noexcept
{
    return socket_;
}

void AsioFramedConnection::ReadHeader()
{
    if (closed_)
    {
        return;
    }

    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(headerBytes_.data(), headerBytes_.size()),
        [self](
            const boost::system::error_code error,
            const std::size_t) {
            if (error)
            {
                self->FinishClose(error);
                return;
            }

            const FrameHeaderDecodeResult decoded =
                DecodeTcpFrameHeader(self->headerBytes_);
            if (!decoded.header.has_value())
            {
                self->FinishClose(HeaderError(decoded.error));
                return;
            }
            self->ReadPayload(*decoded.header);
        });
}

void AsioFramedConnection::ReadPayload(const TcpFrameHeader& header)
{
    payload_.assign(header.payloadBytes, std::byte{0});
    if (payload_.empty())
    {
        DeliverFrame(header.type);
        return;
    }

    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(payload_.data(), payload_.size()),
        [self, type = header.type](
            const boost::system::error_code error,
            const std::size_t) {
            if (error)
            {
                self->FinishClose(error);
                return;
            }
            self->DeliverFrame(type);
        });
}

void AsioFramedConnection::DeliverFrame(const MessageType type)
{
    if (closed_)
    {
        return;
    }

    RawFrame frame{type, std::move(payload_)};
    try
    {
        onFrame_(std::move(frame));
    }
    catch (const std::exception&)
    {
        FinishClose(boost::asio::error::operation_aborted);
        return;
    }
    ReadHeader();
}

void AsioFramedConnection::WriteNext()
{
    if (closed_ || writeQueue_.empty())
    {
        writeInProgress_ = false;
        if (!closed_ && closeAfterFlush_)
        {
            FinishClose(boost::asio::error::operation_aborted);
        }
        return;
    }

    writeInProgress_ = true;
    auto self = shared_from_this();
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(
            writeQueue_.front().data(),
            writeQueue_.front().size()),
        [self](
            const boost::system::error_code error,
            const std::size_t) {
            if (error)
            {
                self->FinishClose(error);
                return;
            }
            if (self->closed_)
            {
                return;
            }

            self->pendingWriteBytes_ -= self->writeQueue_.front().size();
            self->writeQueue_.pop_front();
            self->WriteNext();
        });
}

void AsioFramedConnection::FinishClose(
    const boost::system::error_code error)
{
    if (closed_)
    {
        return;
    }
    closed_ = true;

    const auto executor = socket_.get_executor();
    boost::system::error_code ignored;
    socket_.cancel(ignored);
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);
    pendingWriteBytes_ = 0U;
    writeInProgress_ = false;
    closeAfterFlush_ = false;

    CloseHandler closeHandler = std::move(onClose_);
    if (closeHandler)
    {
        boost::asio::post(
            executor,
            [handler = std::move(closeHandler), error]() mutable {
                handler(error);
            });
    }
}
} // namespace dxa::protocol
