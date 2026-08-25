#pragma once

#include <dxa/protocol/ByteCodec.hpp>
#include <dxa/protocol/GameSnapshot.hpp>

#include <optional>
#include <span>
#include <vector>

namespace dxa::protocol
{
struct GameSnapshotDecodeResult
{
    std::optional<GameSnapshot> snapshot;
    DecodeError error = DecodeError::None;
};

[[nodiscard]] std::vector<std::byte> EncodeGameSnapshot(
    const GameSnapshot& snapshot);
[[nodiscard]] GameSnapshotDecodeResult DecodeGameSnapshot(
    std::span<const std::byte> bytes);
} // namespace dxa::protocol
