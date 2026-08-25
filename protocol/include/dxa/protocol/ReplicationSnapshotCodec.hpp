#pragma once

#include <dxa/protocol/ReplicationSnapshot.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace dxa::protocol
{
enum class SnapshotPayloadDecodeError
{
    None,
    Truncated,
    InvalidValue,
    CountLimit,
    NonCanonicalOrder,
    DuplicateEntity,
    TrailingBytes
};

struct SnapshotPayloadDecodeResult
{
    std::optional<SnapshotPayload> payload;
    SnapshotPayloadDecodeError error = SnapshotPayloadDecodeError::None;
};

[[nodiscard]] std::vector<std::byte> EncodeSnapshotPayload(
    const SnapshotPayload& payload);

[[nodiscard]] SnapshotPayloadDecodeResult DecodeSnapshotPayload(
    std::span<const std::byte> bytes);
} // namespace dxa::protocol
