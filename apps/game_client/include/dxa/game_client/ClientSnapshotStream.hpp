#pragma once

#include <dxa/protocol/GameSnapshot.hpp>
#include <dxa/protocol/ReplicationSnapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace dxa::game_client
{
struct SnapshotApplyResult
{
    std::optional<dxa::protocol::GameSnapshot> world;
    std::vector<dxa::protocol::EntityId> removedActors;
    std::vector<dxa::protocol::EntityId> reenteredActors;
    std::uint32_t acknowledgedSnapshotId = 0U;
    bool requestKeyframe = false;
};

class ClientSnapshotStream
{
public:
    explicit ClientSnapshotStream(
        std::size_t maximumBaselines =
            dxa::protocol::MaxClientSnapshotBuffer);
    ~ClientSnapshotStream();

    ClientSnapshotStream(const ClientSnapshotStream&) = delete;
    ClientSnapshotStream& operator=(const ClientSnapshotStream&) = delete;
    ClientSnapshotStream(ClientSnapshotStream&&) noexcept;
    ClientSnapshotStream& operator=(ClientSnapshotStream&&) noexcept;

    [[nodiscard]] SnapshotApplyResult Apply(
        std::uint32_t snapshotId,
        const dxa::protocol::SnapshotPayload& payload);
    void Reset() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace dxa::game_client
