#pragma once

#include <dxa/game_client/SnapshotReassembler.hpp>

#include <dxa/protocol/GameTypes.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>

namespace dxa::game_client
{
class RemoteInterpolator
{
public:
    RemoteInterpolator(
        std::uint32_t interpolationDelayTicks = 3U,
        std::size_t capacity = dxa::protocol::MaxClientSnapshotBuffer);

    void Push(ReassembledSnapshot snapshot);
    void ForgetActors(
        std::span<const dxa::protocol::EntityId> actors);
    [[nodiscard]] dxa::protocol::GameSnapshot Sample(
        std::optional<dxa::protocol::EntityId> localActor = std::nullopt) const;
    [[nodiscard]] std::size_t BufferSize() const noexcept;

private:
    std::uint32_t interpolationDelayTicks_ = 3U;
    std::size_t capacity_ = dxa::protocol::MaxClientSnapshotBuffer;
    std::deque<ReassembledSnapshot> snapshots_;
};
} // namespace dxa::game_client
