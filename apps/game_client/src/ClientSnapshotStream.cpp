#include <dxa/game_client/ClientSnapshotStream.hpp>

#include <dxa/protocol/ReplicationSnapshotCodec.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <utility>

namespace dxa::game_client
{
namespace
{
using namespace dxa::protocol;

inline constexpr float ArenaMinimum = -128.0F;
inline constexpr float ArenaMaximum = 128.0F;
inline constexpr float SafeZoneRadiusMaximum = 128.0F;

struct QuantizedWorld
{
    QuantizedGlobalValue global;
    std::vector<QuantizedActorValue> actors;
    std::vector<QuantizedLootValue> loot;
};

template <typename Field>
[[nodiscard]] bool HasField(const Field fields, const Field field) noexcept
{
    return (static_cast<std::uint8_t>(fields)
            & static_cast<std::uint8_t>(field))
        != 0U;
}

[[nodiscard]] QuantizedWorld WorldFromKeyframe(
    const SnapshotPayload& payload)
{
    return QuantizedWorld{
        payload.global,
        payload.actorValues,
        payload.lootValues};
}

[[nodiscard]] GameSnapshot DequantizeWorld(const QuantizedWorld& source)
{
    GameSnapshot world;
    world.phase = source.global.phase;
    world.safeZoneStage = source.global.safeZoneStage;
    world.safeZoneCenter = {
        DequantizeCoordinate(
            source.global.safeZoneCenter.x,
            ArenaMinimum,
            ArenaMaximum),
        DequantizeCoordinate(
            source.global.safeZoneCenter.z,
            ArenaMinimum,
            ArenaMaximum)};
    world.safeZoneRadius = DequantizeSafeZoneRadius(
        source.global.safeZoneRadius,
        SafeZoneRadiusMaximum);
    world.aliveContenders = source.global.aliveContenders;
    world.result = source.global.result;
    world.hasResult = source.global.hasResult;
    world.eventChecksum = source.global.eventChecksum;

    world.actors.reserve(source.actors.size());
    for (const QuantizedActorValue& actor : source.actors)
    {
        world.actors.push_back(NetworkActorSnapshot{
            actor.id,
            actor.role,
            actor.neutralArchetype,
            {DequantizeCoordinate(
                 actor.position.x,
                 ArenaMinimum,
                 ArenaMaximum),
             DequantizeCoordinate(
                 actor.position.z,
                 ArenaMinimum,
                 ArenaMaximum)},
            actor.health,
            actor.alive,
            actor.weapon,
            actor.cooldownTicksRemaining,
            actor.eliminations});
    }
    world.loot.reserve(source.loot.size());
    for (const QuantizedLootValue& loot : source.loot)
    {
        world.loot.push_back(NetworkLootSnapshot{
            loot.id,
            loot.type,
            {DequantizeCoordinate(
                 loot.position.x,
                 ArenaMinimum,
                 ArenaMaximum),
             DequantizeCoordinate(
                 loot.position.z,
                 ArenaMinimum,
                 ArenaMaximum)},
            loot.active});
    }
    return world;
}

template <typename Value, typename Id, typename Key>
[[nodiscard]] auto FindById(
    std::vector<Value>& values,
    const Id id,
    Key key)
{
    return std::lower_bound(
        values.begin(),
        values.end(),
        id,
        [key](const Value& value, const Id target) {
            return key(value) < target;
        });
}

void ApplyGlobalDelta(
    QuantizedGlobalValue& target,
    const QuantizedGlobalDelta& delta)
{
    if (HasField(delta.fields, GlobalField::Phase))
    {
        target.phase = delta.phase;
    }
    if (HasField(delta.fields, GlobalField::SafeZone))
    {
        target.safeZoneStage = delta.safeZoneStage;
        target.safeZoneCenter = delta.safeZoneCenter;
        target.safeZoneRadius = delta.safeZoneRadius;
    }
    if (HasField(delta.fields, GlobalField::AliveContenders))
    {
        target.aliveContenders = delta.aliveContenders;
    }
    if (HasField(delta.fields, GlobalField::Result))
    {
        target.result = delta.result;
        target.hasResult = delta.hasResult;
    }
    if (HasField(delta.fields, GlobalField::EventChecksum))
    {
        target.eventChecksum = delta.eventChecksum;
    }
}

void RemoveActors(
    std::vector<QuantizedActorValue>& actors,
    const std::vector<EntityId>& removed)
{
    for (const EntityId id : removed)
    {
        const auto found = FindById(
            actors,
            id,
            [](const QuantizedActorValue& actor) { return actor.id; });
        if (found == actors.end() || found->id != id)
        {
            throw std::invalid_argument{"removed actor is not in baseline"};
        }
        actors.erase(found);
    }
}

void EnterActors(
    std::vector<QuantizedActorValue>& actors,
    const std::vector<QuantizedActorValue>& entered)
{
    for (const QuantizedActorValue& actor : entered)
    {
        const auto insertion = FindById(
            actors,
            actor.id,
            [](const QuantizedActorValue& value) { return value.id; });
        if (insertion != actors.end() && insertion->id == actor.id)
        {
            throw std::invalid_argument{"entered actor already exists"};
        }
        actors.insert(insertion, actor);
    }
}

void ChangeActors(
    std::vector<QuantizedActorValue>& actors,
    const std::vector<QuantizedActorDelta>& changes)
{
    for (const QuantizedActorDelta& change : changes)
    {
        const auto found = FindById(
            actors,
            change.id,
            [](const QuantizedActorValue& actor) { return actor.id; });
        if (found == actors.end() || found->id != change.id)
        {
            throw std::invalid_argument{"actor delta target is missing"};
        }
        if (HasField(change.fields, ActorField::Position))
        {
            found->position = change.position;
        }
        if (HasField(change.fields, ActorField::HealthAlive))
        {
            found->health = change.health;
            found->alive = change.alive;
        }
        if (HasField(change.fields, ActorField::WeaponCooldown))
        {
            found->weapon = change.weapon;
            found->cooldownTicksRemaining = change.cooldownTicksRemaining;
        }
        if (HasField(change.fields, ActorField::Eliminations))
        {
            found->eliminations = change.eliminations;
        }
    }
}

void RemoveLoot(
    std::vector<QuantizedLootValue>& loot,
    const std::vector<std::uint32_t>& removed)
{
    for (const std::uint32_t id : removed)
    {
        const auto found = FindById(
            loot,
            id,
            [](const QuantizedLootValue& value) { return value.id; });
        if (found == loot.end() || found->id != id)
        {
            throw std::invalid_argument{"removed loot is not in baseline"};
        }
        loot.erase(found);
    }
}

void EnterLoot(
    std::vector<QuantizedLootValue>& loot,
    const std::vector<QuantizedLootValue>& entered)
{
    for (const QuantizedLootValue& value : entered)
    {
        const auto insertion = FindById(
            loot,
            value.id,
            [](const QuantizedLootValue& candidate) {
                return candidate.id;
            });
        if (insertion != loot.end() && insertion->id == value.id)
        {
            throw std::invalid_argument{"entered loot already exists"};
        }
        loot.insert(insertion, value);
    }
}

void ChangeLoot(
    std::vector<QuantizedLootValue>& loot,
    const std::vector<QuantizedLootDelta>& changes)
{
    for (const QuantizedLootDelta& change : changes)
    {
        const auto found = FindById(
            loot,
            change.id,
            [](const QuantizedLootValue& value) { return value.id; });
        if (found == loot.end() || found->id != change.id)
        {
            throw std::invalid_argument{"loot delta target is missing"};
        }
        found->active = change.active;
    }
}

[[nodiscard]] QuantizedWorld ApplyDelta(
    const QuantizedWorld& base,
    const SnapshotPayload& payload)
{
    QuantizedWorld applied = base;
    ApplyGlobalDelta(applied.global, payload.globalDelta);
    RemoveActors(applied.actors, payload.removedActors);
    EnterActors(applied.actors, payload.actorValues);
    ChangeActors(applied.actors, payload.actorDeltas);
    RemoveLoot(applied.loot, payload.removedLoot);
    EnterLoot(applied.loot, payload.lootValues);
    ChangeLoot(applied.loot, payload.lootDeltas);
    return applied;
}

[[nodiscard]] std::vector<EntityId> ActorIds(
    const GameSnapshot& world)
{
    std::vector<EntityId> ids;
    ids.reserve(world.actors.size());
    for (const NetworkActorSnapshot& actor : world.actors)
    {
        ids.push_back(actor.id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

void FindActorTransitions(
    const std::optional<GameSnapshot>& previous,
    const GameSnapshot& current,
    std::vector<EntityId>& removed,
    std::vector<EntityId>& entered)
{
    const std::vector<EntityId> before = previous.has_value()
        ? ActorIds(*previous)
        : std::vector<EntityId>{};
    const std::vector<EntityId> after = ActorIds(current);
    std::set_difference(
        before.begin(),
        before.end(),
        after.begin(),
        after.end(),
        std::back_inserter(removed));
    std::set_difference(
        after.begin(),
        after.end(),
        before.begin(),
        before.end(),
        std::back_inserter(entered));
}
} // namespace

struct ClientSnapshotStream::Impl
{
    struct Baseline
    {
        std::uint32_t snapshotId = 0U;
        std::optional<QuantizedWorld> quantized;
        GameSnapshot world;
    };

    explicit Impl(const std::size_t requestedMaximum)
        : maximumBaselines{requestedMaximum}
    {
        if (maximumBaselines == 0U
            || maximumBaselines > MaxClientSnapshotBuffer)
        {
            throw std::invalid_argument{
                "client snapshot baseline capacity is invalid"};
        }
    }

    [[nodiscard]] const Baseline* FindBaseline(
        const std::uint32_t snapshotId) const
    {
        const auto found = std::find_if(
            baselines.begin(),
            baselines.end(),
            [snapshotId](const Baseline& baseline) {
                return baseline.snapshotId == snapshotId;
            });
        return found == baselines.end() ? nullptr : &*found;
    }

    [[nodiscard]] std::optional<GameSnapshot> LatestWorld() const
    {
        return baselines.empty()
            ? std::nullopt
            : std::optional<GameSnapshot>{baselines.back().world};
    }

    void Store(Baseline baseline)
    {
        baselines.push_back(std::move(baseline));
        while (baselines.size() > maximumBaselines)
        {
            baselines.pop_front();
        }
    }

    [[nodiscard]] SnapshotApplyResult CurrentResult() const
    {
        SnapshotApplyResult result;
        result.acknowledgedSnapshotId = acknowledgedSnapshotId;
        result.requestKeyframe = requestKeyframe;
        return result;
    }

    std::size_t maximumBaselines = MaxClientSnapshotBuffer;
    std::deque<Baseline> baselines;
    std::optional<std::uint32_t> highestSeenSnapshotId;
    std::uint32_t acknowledgedSnapshotId = 0U;
    bool requestKeyframe = false;
};

ClientSnapshotStream::ClientSnapshotStream(const std::size_t maximumBaselines)
    : impl_{std::make_unique<Impl>(maximumBaselines)}
{
}

ClientSnapshotStream::~ClientSnapshotStream() = default;
ClientSnapshotStream::ClientSnapshotStream(ClientSnapshotStream&&) noexcept =
    default;
ClientSnapshotStream& ClientSnapshotStream::operator=(
    ClientSnapshotStream&&) noexcept = default;

SnapshotApplyResult ClientSnapshotStream::Apply(
    const std::uint32_t snapshotId,
    const SnapshotPayload& payload)
{
    if (impl_ == nullptr)
    {
        throw std::logic_error{"client snapshot stream was moved from"};
    }
    if (snapshotId == 0U
        || payload.header.payloadSnapshotId != snapshotId)
    {
        throw std::invalid_argument{"snapshot payload identity mismatch"};
    }
    (void)EncodeSnapshotPayload(payload);

    Impl& state = *impl_;
    if (state.highestSeenSnapshotId.has_value()
        && snapshotId <= *state.highestSeenSnapshotId)
    {
        return state.CurrentResult();
    }
    state.highestSeenSnapshotId = snapshotId;

    std::optional<QuantizedWorld> quantized;
    GameSnapshot world;
    SnapshotApplyResult result;
    if (payload.header.kind == SnapshotPayloadKind::FullState
        || (payload.header.kind == SnapshotPayloadKind::Keyframe
            && payload.header.valueEncoding
                == SnapshotValueEncoding::FullPrecision))
    {
        world = payload.fullPrecision;
        FindActorTransitions(
            state.LatestWorld(),
            world,
            result.removedActors,
            result.reenteredActors);
    }
    else if (payload.header.kind == SnapshotPayloadKind::Keyframe)
    {
        quantized = WorldFromKeyframe(payload);
        world = DequantizeWorld(*quantized);
        FindActorTransitions(
            state.LatestWorld(),
            world,
            result.removedActors,
            result.reenteredActors);
    }
    else
    {
        const Impl::Baseline* baseline = state.FindBaseline(
            payload.header.baseSnapshotId);
        if (baseline == nullptr || !baseline->quantized.has_value())
        {
            state.requestKeyframe = true;
            return state.CurrentResult();
        }
        quantized = ApplyDelta(*baseline->quantized, payload);
        world = DequantizeWorld(*quantized);
        result.removedActors = payload.removedActors;
        result.reenteredActors.reserve(payload.actorValues.size());
        for (const QuantizedActorValue& actor : payload.actorValues)
        {
            result.reenteredActors.push_back(actor.id);
        }
    }

    if (state.requestKeyframe
        && payload.header.kind == SnapshotPayloadKind::Keyframe)
    {
        result.reenteredActors = ActorIds(world);
    }

    state.Store(Impl::Baseline{
        snapshotId,
        std::move(quantized),
        world});
    state.acknowledgedSnapshotId = snapshotId;
    state.requestKeyframe = false;
    result.world = std::move(world);
    result.acknowledgedSnapshotId = snapshotId;
    result.requestKeyframe = false;
    return result;
}

void ClientSnapshotStream::Reset() noexcept
{
    if (impl_)
    {
        impl_->baselines.clear();
        impl_->highestSeenSnapshotId.reset();
        impl_->acknowledgedSnapshotId = 0U;
        impl_->requestKeyframe = false;
    }
}
} // namespace dxa::game_client
