#pragma once

#include <dxa/protocol/GameSnapshot.hpp>
#include <dxa/protocol/GameUdpMessages.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace dxa::game_client
{
struct ReassembledPayload
{
    std::uint32_t snapshotId = 0U;
    std::uint32_t serverTick = 0U;
    std::uint32_t ackInputSequence = 0U;
    std::vector<std::byte> bytes;
};

struct ReassembledSnapshot
{
    std::uint32_t snapshotId = 0U;
    std::uint32_t serverTick = 0U;
    std::uint32_t ackInputSequence = 0U;
    dxa::protocol::GameSnapshot snapshot;
};

class SnapshotReassembler
{
public:
    SnapshotReassembler();
    ~SnapshotReassembler();
    SnapshotReassembler(SnapshotReassembler&&) noexcept;
    SnapshotReassembler& operator=(SnapshotReassembler&&) noexcept;
    SnapshotReassembler(const SnapshotReassembler&) = delete;
    SnapshotReassembler& operator=(const SnapshotReassembler&) = delete;

    [[nodiscard]] std::optional<ReassembledPayload> PushBytes(
        const dxa::protocol::SnapshotFragment& fragment);
    [[nodiscard]] bool TakeRecoveryNeeded() noexcept;
    void Reset() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace dxa::game_client
