#include <dxa/protocol/TcpFrame.hpp>

#include <dxa/protocol/ByteCodec.hpp>

#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace dxa::protocol
{
namespace
{
constexpr std::array TcpMagic{
    std::byte{0x44},
    std::byte{0x58},
    std::byte{0x41},
    std::byte{0x31}};

[[nodiscard]] bool IsKnownMessageType(const std::uint16_t value) noexcept
{
    switch (static_cast<MessageType>(value))
    {
    case MessageType::ClientHello:
    case MessageType::ServerWelcome:
    case MessageType::ListRoomsRequest:
    case MessageType::RoomListResponse:
    case MessageType::CreateRoomRequest:
    case MessageType::JoinRoomRequest:
    case MessageType::LeaveRoomRequest:
    case MessageType::SetReadyRequest:
    case MessageType::StartMatchRequest:
    case MessageType::RoomSnapshot:
    case MessageType::MatchTicket:
    case MessageType::ErrorResponse:
        return true;
    }
    return false;
}
} // namespace

std::vector<std::byte> EncodeTcpFrame(const EncodedMessage& message)
{
    if (!IsKnownMessageType(static_cast<std::uint16_t>(message.type)))
    {
        throw std::invalid_argument{"TCP frame message type is unknown"};
    }
    if (message.payload.size() > MaxTcpPayloadBytes
        || message.payload.size() > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::length_error{"TCP frame payload exceeds 64 KiB frame limit"};
    }

    ByteWriter writer;
    writer.WriteBytes(TcpMagic);
    writer.WriteU16(ProtocolVersion);
    writer.WriteU16(static_cast<std::uint16_t>(message.type));
    writer.WriteU32(static_cast<std::uint32_t>(message.payload.size()));
    writer.WriteBytes(message.payload);
    return std::move(writer).Finish();
}

FrameHeaderDecodeResult DecodeTcpFrameHeader(
    const std::span<const std::byte> bytes) noexcept
{
    if (bytes.size() != TcpFrameHeaderBytes)
    {
        return {std::nullopt, FrameHeaderError::InvalidHeaderSize};
    }
    for (std::size_t index = 0; index < TcpMagic.size(); ++index)
    {
        if (bytes[index] != TcpMagic[index])
        {
            return {std::nullopt, FrameHeaderError::BadMagic};
        }
    }

    ByteReader reader{bytes.subspan(TcpMagic.size())};
    const std::optional<std::uint16_t> version = reader.ReadU16();
    const std::optional<std::uint16_t> type = reader.ReadU16();
    const std::optional<std::uint32_t> payloadBytes = reader.ReadU32();
    if (!version.has_value() || !type.has_value() || !payloadBytes.has_value())
    {
        return {std::nullopt, FrameHeaderError::InvalidHeaderSize};
    }
    if (*version != ProtocolVersion)
    {
        return {std::nullopt, FrameHeaderError::UnsupportedVersion};
    }
    if (!IsKnownMessageType(*type))
    {
        return {std::nullopt, FrameHeaderError::UnknownMessageType};
    }
    if (*payloadBytes > MaxTcpPayloadBytes)
    {
        return {std::nullopt, FrameHeaderError::FrameTooLarge};
    }

    return {
        TcpFrameHeader{
            *version,
            static_cast<MessageType>(*type),
            *payloadBytes},
        FrameHeaderError::None};
}
} // namespace dxa::protocol
