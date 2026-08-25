#pragma once

#include <dxa/protocol/GameSnapshot.hpp>
#include <dxa/protocol/GameTypes.hpp>
#include <dxa/protocol/ReplicationSnapshot.hpp>
#include <dxa/simulation/ArenaMap.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace dxa::game_server
{
struct ReplicationConfig
{
    dxa::protocol::ReplicationMode mode =
        dxa::protocol::ReplicationMode::FullState;
    float cellSize = 32.0F;
    float enterRadius = 80.0F;
    float leaveRadius = 88.0F;
    std::uint32_t keyframeIntervalSnapshots = 30U;
    std::size_t maximumBaselinesPerRecipient = 32U;
};

struct ReplicationBuild
{
    dxa::protocol::SnapshotPayload payload;
    std::uint32_t visibleActorCount = 0U;
    std::uint32_t visibleLootCount = 0U;
    bool keyframe = false;
    bool fallbackKeyframe = false;
};

class SnapshotReplicator
{
public:
    SnapshotReplicator(
        const dxa::simulation::ArenaMapDefinition& arena,
        ReplicationConfig config);
    ~SnapshotReplicator();

    SnapshotReplicator(const SnapshotReplicator&) = delete;
    SnapshotReplicator& operator=(const SnapshotReplicator&) = delete;
    SnapshotReplicator(SnapshotReplicator&&) noexcept;
    SnapshotReplicator& operator=(SnapshotReplicator&&) noexcept;

    void RegisterRecipient(
        dxa::protocol::PlayerId player,
        dxa::protocol::EntityId controlledActor);
    [[nodiscard]] bool AcceptAcknowledgement(
        dxa::protocol::PlayerId player,
        std::uint32_t snapshotId);
    void RequestKeyframe(dxa::protocol::PlayerId player);
    [[nodiscard]] ReplicationBuild Build(
        dxa::protocol::PlayerId player,
        std::uint32_t snapshotId,
        const dxa::protocol::GameSnapshot& world);
    void RemoveRecipient(dxa::protocol::PlayerId player);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace dxa::game_server
