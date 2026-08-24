#pragma once

#include <dxa/protocol/ByteCodec.hpp>
#include <dxa/protocol/LobbyMessages.hpp>
#include <dxa/protocol/TcpFrame.hpp>

#include <optional>
#include <span>

namespace dxa::protocol
{
template <typename MessageVariant>
struct MessageDecodeResult
{
    std::optional<MessageVariant> message;
    DecodeError error = DecodeError::None;
};

[[nodiscard]] EncodedMessage EncodeClientMessage(const ClientMessage& message);
[[nodiscard]] EncodedMessage EncodeServerMessage(const ServerMessage& message);
[[nodiscard]] MessageDecodeResult<ClientMessage> DecodeClientMessage(
    MessageType type,
    std::span<const std::byte> payload);
[[nodiscard]] MessageDecodeResult<ServerMessage> DecodeServerMessage(
    MessageType type,
    std::span<const std::byte> payload);
} // namespace dxa::protocol
