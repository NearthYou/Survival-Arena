#pragma once

#include <dxa/protocol/ByteCodec.hpp>
#include <dxa/protocol/GameUdpMessages.hpp>

#include <optional>
#include <span>
#include <vector>

namespace dxa::protocol
{
template <typename DatagramVariant>
struct DatagramDecodeResult
{
    std::optional<DatagramVariant> datagram;
    DecodeError error = DecodeError::None;
};

[[nodiscard]] EncodedDatagram EncodeClientDatagram(
    const ClientDatagram& datagram);
[[nodiscard]] EncodedDatagram EncodeServerDatagram(
    const ServerDatagram& datagram);
[[nodiscard]] DatagramDecodeResult<ClientDatagram> DecodeClientDatagram(
    std::span<const std::byte> bytes);
[[nodiscard]] DatagramDecodeResult<ServerDatagram> DecodeServerDatagram(
    std::span<const std::byte> bytes);
[[nodiscard]] std::vector<SnapshotFragment> FragmentSnapshot(
    MatchId match,
    std::uint32_t snapshotId,
    std::uint32_t serverTick,
    std::uint32_t ackInputSequence,
    std::span<const std::byte> fullPayload);
} // namespace dxa::protocol
