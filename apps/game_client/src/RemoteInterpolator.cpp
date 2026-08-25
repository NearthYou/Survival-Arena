#include <dxa/game_client/RemoteInterpolator.hpp>

#include <dxa/protocol/GameSnapshot.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace dxa::game_client
{
namespace
{
void ExcludeLocalActor(
    dxa::protocol::GameSnapshot& snapshot,
    const std::optional<dxa::protocol::EntityId> localActor)
{
    if (!localActor.has_value())
    {
        return;
    }
    const auto actor = std::find_if(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        [localActor](const auto& candidate) {
            return candidate.id == *localActor;
        });
    if (actor == snapshot.actors.end())
    {
        return;
    }
    if (actor->role == dxa::protocol::NetworkActorRole::Contender
        && actor->alive
        && snapshot.aliveContenders > 0U)
    {
        --snapshot.aliveContenders;
    }
    snapshot.actors.erase(actor);
}
} // namespace

RemoteInterpolator::RemoteInterpolator(
    const std::uint32_t interpolationDelayTicks,
    const std::size_t capacity)
    : interpolationDelayTicks_{interpolationDelayTicks},
      capacity_{capacity}
{
    if (capacity_ == 0U
        || capacity_ > dxa::protocol::MaxClientSnapshotBuffer)
    {
        throw std::invalid_argument{
            "remote interpolation capacity is invalid"};
    }
}

void RemoteInterpolator::Push(ReassembledSnapshot snapshot)
{
    if (!snapshots_.empty()
        && (snapshot.serverTick <= snapshots_.back().serverTick
            || snapshot.snapshotId <= snapshots_.back().snapshotId))
    {
        return;
    }
    snapshots_.push_back(std::move(snapshot));
    while (snapshots_.size() > capacity_)
    {
        snapshots_.pop_front();
    }
}

dxa::protocol::GameSnapshot RemoteInterpolator::Sample(
    const std::optional<dxa::protocol::EntityId> localActor) const
{
    if (snapshots_.empty())
    {
        throw std::logic_error{"remote interpolation buffer is empty"};
    }
    const std::uint32_t newestTick = snapshots_.back().serverTick;
    const std::uint32_t targetTick = newestTick > interpolationDelayTicks_
        ? newestTick - interpolationDelayTicks_
        : 0U;

    if (targetTick <= snapshots_.front().serverTick)
    {
        dxa::protocol::GameSnapshot held = snapshots_.front().snapshot;
        ExcludeLocalActor(held, localActor);
        return held;
    }
    if (targetTick >= snapshots_.back().serverTick)
    {
        dxa::protocol::GameSnapshot held = snapshots_.back().snapshot;
        ExcludeLocalActor(held, localActor);
        return held;
    }

    const auto newer = std::lower_bound(
        snapshots_.begin(),
        snapshots_.end(),
        targetTick,
        [](const ReassembledSnapshot& snapshot, const std::uint32_t tick) {
            return snapshot.serverTick < tick;
        });
    if (newer == snapshots_.end())
    {
        dxa::protocol::GameSnapshot held = snapshots_.back().snapshot;
        ExcludeLocalActor(held, localActor);
        return held;
    }
    if (newer->serverTick == targetTick || newer == snapshots_.begin())
    {
        dxa::protocol::GameSnapshot held = newer->snapshot;
        ExcludeLocalActor(held, localActor);
        return held;
    }

    const auto older = std::prev(newer);
    const float alpha = static_cast<float>(
        targetTick - older->serverTick)
        / static_cast<float>(newer->serverTick - older->serverTick);
    dxa::protocol::GameSnapshot interpolated = newer->snapshot;
    for (dxa::protocol::NetworkActorSnapshot& actor : interpolated.actors)
    {
        const auto source = std::lower_bound(
            older->snapshot.actors.begin(),
            older->snapshot.actors.end(),
            actor.id,
            [](const dxa::protocol::NetworkActorSnapshot& candidate,
               const dxa::protocol::EntityId id) {
                return candidate.id < id;
            });
        if (source == older->snapshot.actors.end() || source->id != actor.id)
        {
            continue;
        }
        actor.position.x = source->position.x
            + (actor.position.x - source->position.x) * alpha;
        actor.position.z = source->position.z
            + (actor.position.z - source->position.z) * alpha;
    }
    ExcludeLocalActor(interpolated, localActor);
    return interpolated;
}

std::size_t RemoteInterpolator::BufferSize() const noexcept
{
    return snapshots_.size();
}
} // namespace dxa::game_client
