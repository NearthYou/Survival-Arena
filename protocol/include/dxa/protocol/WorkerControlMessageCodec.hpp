#pragma once

#include <dxa/protocol/MessageCodec.hpp>
#include <dxa/protocol/TcpFrame.hpp>
#include <dxa/protocol/WorkerControlMessages.hpp>

#include <span>

namespace dxa::protocol
{
[[nodiscard]] EncodedMessage EncodeWorkerToLobbyMessage(
    const WorkerToLobbyMessage& message);
[[nodiscard]] EncodedMessage EncodeLobbyToWorkerMessage(
    const LobbyToWorkerMessage& message);
[[nodiscard]] MessageDecodeResult<WorkerToLobbyMessage>
DecodeWorkerToLobbyMessage(
    MessageType type,
    std::span<const std::byte> payload);
[[nodiscard]] MessageDecodeResult<LobbyToWorkerMessage>
DecodeLobbyToWorkerMessage(
    MessageType type,
    std::span<const std::byte> payload);
} // namespace dxa::protocol
