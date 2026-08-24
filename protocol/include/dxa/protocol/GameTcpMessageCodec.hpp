#pragma once

#include <dxa/protocol/GameTcpMessages.hpp>
#include <dxa/protocol/MessageCodec.hpp>
#include <dxa/protocol/TcpFrame.hpp>

#include <span>

namespace dxa::protocol
{
[[nodiscard]] EncodedMessage EncodeGameClientMessage(
    const GameClientMessage& message);
[[nodiscard]] EncodedMessage EncodeGameServerMessage(
    const GameServerMessage& message);
[[nodiscard]] MessageDecodeResult<GameClientMessage> DecodeGameClientMessage(
    MessageType type,
    std::span<const std::byte> payload);
[[nodiscard]] MessageDecodeResult<GameServerMessage> DecodeGameServerMessage(
    MessageType type,
    std::span<const std::byte> payload);
} // namespace dxa::protocol
