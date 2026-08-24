#pragma once

#include <dxa/protocol/LobbyTypes.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace dxa::protocol
{
struct EncodedMessage
{
    MessageType type = MessageType::ClientHello;
    std::vector<std::byte> payload;

    [[nodiscard]] bool operator==(const EncodedMessage&) const = default;
};

struct TcpFrameHeader
{
    std::uint16_t version = ProtocolVersion;
    MessageType type = MessageType::ClientHello;
    std::uint32_t payloadBytes = 0;

    [[nodiscard]] bool operator==(const TcpFrameHeader&) const = default;
};

enum class FrameHeaderError
{
    None,
    InvalidHeaderSize,
    BadMagic,
    UnsupportedVersion,
    UnknownMessageType,
    FrameTooLarge
};

struct FrameHeaderDecodeResult
{
    std::optional<TcpFrameHeader> header;
    FrameHeaderError error = FrameHeaderError::None;
};

[[nodiscard]] std::vector<std::byte> EncodeTcpFrame(const EncodedMessage& message);
[[nodiscard]] FrameHeaderDecodeResult DecodeTcpFrameHeader(
    std::span<const std::byte> bytes) noexcept;
} // namespace dxa::protocol
