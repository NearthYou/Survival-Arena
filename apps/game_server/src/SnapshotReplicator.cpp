#include <dxa/game_server/SnapshotReplicator.hpp>

#include <dxa/game_server/InterestGrid.hpp>
#include <dxa/protocol/ReplicationSnapshotCodec.hpp>
#include <dxa/simulation/Math2.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>

namespace dxa::game_server
{
namespace
{
using namespace dxa::protocol;
using dxa::simulation::Aabb2;
using dxa::simulation::ArenaMapDefinition;
using dxa::simulation::Vec2;

[[nodiscard]] bool IsValidMode(const ReplicationMode mode) noexcept
{
    switch (mode)
    {
    case ReplicationMode::FullState:
    case ReplicationMode::InterestFullPrecision:
    case ReplicationMode::InterestQuantized:
    case ReplicationMode::InterestDelta:
        return true;
    }
    return false;
}

[[nodiscard]] ReplicationConfig ValidateConfig(ReplicationConfig config)
{
    if (!IsValidMode(config.mode)
        || !std::isfinite(config.cellSize)
        || !std::isfinite(config.enterRadius)
        || !std::isfinite(config.leaveRadius)
        || config.cellSize <= 0.0F
        || config.enterRadius <= 0.0F
        || config.leaveRadius < config.enterRadius
        || config.keyframeIntervalSnapshots == 0U
        || config.maximumBaselinesPerRecipient == 0U
        || config.maximumBaselinesPerRecipient > 32U)
    {
        throw std::invalid_argument{"replication configuration is invalid"};
    }
    return config;
}

[[nodiscard]] Aabb2 ArenaBounds(const ArenaMapDefinition& arena)
{
    if (arena.vertices.empty())
    {
        throw std::invalid_argument{"replication arena has no vertices"};
    }

    Vec2 minimum = arena.vertices.front();
    Vec2 maximum = arena.vertices.front();
    for (const Vec2 vertex : arena.vertices)
    {
        if (!dxa::simulation::IsFinite(vertex))
        {
            throw std::invalid_argument{"replication arena is not finite"};
        }
        minimum.x = std::min(minimum.x, vertex.x);
        minimum.z = std::min(minimum.z, vertex.z);
        maximum.x = std::max(maximum.x, vertex.x);
        maximum.z = std::max(maximum.z, vertex.z);
    }
    if (minimum.x >= maximum.x || minimum.z >= maximum.z)
    {
        throw std::invalid_argument{"replication arena has no area"};
    }
    return Aabb2::Create(minimum, maximum);
}

[[nodiscard]] const NetworkActorSnapshot* FindActor(
    const GameSnapshot& world,
    const EntityId id)
{
    const auto found = std::lower_bound(
        world.actors.begin(),
        world.actors.end(),
        id,
        [](const NetworkActorSnapshot& actor, const EntityId value) {
            return actor.id < value;
        });
    return found == world.actors.end() || found->id != id
        ? nullptr
        : &*found;
}

[[nodiscard]] const NetworkLootSnapshot* FindLoot(
    const GameSnapshot& world,
    const std::uint32_t id)
{
    const auto found = std::lower_bound(
        world.loot.begin(),
        world.loot.end(),
        id,
        [](const NetworkLootSnapshot& loot, const std::uint32_t value) {
            return loot.id < value;
        });
    return found == world.loot.end() || found->id != id
        ? nullptr
        : &*found;
}

template <typename Value, typename Key>
[[nodiscard]] bool HasCanonicalIds(
    const std::vector<Value>& values,
    Key key)
{
    for (std::size_t index = 1U; index < values.size(); ++index)
    {
        if (!(key(values[index - 1U]) < key(values[index])))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool HasCanonicalWorldIds(const GameSnapshot& world)
{
    return HasCanonicalIds(
               world.actors,
               [](const NetworkActorSnapshot& actor) { return actor.id; })
        && HasCanonicalIds(
            world.loot,
            [](const NetworkLootSnapshot& loot) { return loot.id; });
}

[[nodiscard]] bool Contains(
    const std::vector<EntityId>& values,
    const EntityId value)
{
    return std::binary_search(values.begin(), values.end(), value);
}

void CopyGlobalState(const GameSnapshot& source, GameSnapshot& destination)
{
    destination.phase = source.phase;
    destination.safeZoneStage = source.safeZoneStage;
    destination.safeZoneCenter = source.safeZoneCenter;
    destination.safeZoneRadius = source.safeZoneRadius;
    destination.aliveContenders = source.aliveContenders;
    destination.result = source.result;
    destination.hasResult = source.hasResult;
    destination.eventChecksum = source.eventChecksum;
}

[[nodiscard]] GameSnapshot FilterWorld(
    const GameSnapshot& world,
    const VisibleSet& visible)
{
    GameSnapshot filtered;
    CopyGlobalState(world, filtered);
    filtered.actors.reserve(visible.actors.size());
    for (const EntityId id : visible.actors)
    {
        const NetworkActorSnapshot* actor = FindActor(world, id);
        if (actor == nullptr)
        {
            throw std::logic_error{"visible actor is missing from world"};
        }
        filtered.actors.push_back(*actor);
    }
    filtered.loot.reserve(visible.loot.size());
    for (const std::uint32_t id : visible.loot)
    {
        const NetworkLootSnapshot* loot = FindLoot(world, id);
        if (loot == nullptr || !loot->active)
        {
            throw std::logic_error{"visible loot is missing from world"};
        }
        filtered.loot.push_back(*loot);
    }
    return filtered;
}

[[nodiscard]] QuantizedActorValue QuantizeActor(
    const NetworkActorSnapshot& actor,
    const Vec2 minimum,
    const Vec2 maximum)
{
    return QuantizedActorValue{
        actor.id,
        actor.role,
        actor.neutralArchetype,
        {QuantizeCoordinate(actor.position.x, minimum.x, maximum.x),
         QuantizeCoordinate(actor.position.z, minimum.z, maximum.z)},
        QuantizeHealth(actor.health),
        actor.alive,
        actor.weapon,
        QuantizeCooldownTicks(actor.cooldownTicksRemaining),
        QuantizeEliminations(actor.eliminations)};
}

[[nodiscard]] QuantizedLootValue QuantizeLoot(
    const NetworkLootSnapshot& loot,
    const Vec2 minimum,
    const Vec2 maximum)
{
    return QuantizedLootValue{
        loot.id,
        loot.type,
        {QuantizeCoordinate(loot.position.x, minimum.x, maximum.x),
         QuantizeCoordinate(loot.position.z, minimum.z, maximum.z)},
        loot.active};
}

[[nodiscard]] QuantizedGlobalValue QuantizeGlobal(
    const GameSnapshot& world,
    const Vec2 minimum,
    const Vec2 maximum,
    const float radiusMaximum)
{
    return QuantizedGlobalValue{
        world.phase,
        world.safeZoneStage,
        {QuantizeCoordinate(
             world.safeZoneCenter.x,
             minimum.x,
             maximum.x),
         QuantizeCoordinate(
             world.safeZoneCenter.z,
             minimum.z,
             maximum.z)},
        QuantizeSafeZoneRadius(world.safeZoneRadius, radiusMaximum),
        QuantizeAliveContenders(world.aliveContenders),
        world.result,
        world.hasResult,
        world.eventChecksum};
}

struct QuantizedView
{
    QuantizedGlobalValue global;
    std::vector<QuantizedActorValue> actors;
    std::vector<QuantizedLootValue> loot;
};

[[nodiscard]] QuantizedView QuantizeView(
    const GameSnapshot& filtered,
    const Vec2 minimum,
    const Vec2 maximum,
    const float radiusMaximum)
{
    QuantizedView view;
    view.global = QuantizeGlobal(
        filtered,
        minimum,
        maximum,
        radiusMaximum);
    view.actors.reserve(filtered.actors.size());
    for (const NetworkActorSnapshot& actor : filtered.actors)
    {
        view.actors.push_back(QuantizeActor(actor, minimum, maximum));
    }
    view.loot.reserve(filtered.loot.size());
    for (const NetworkLootSnapshot& loot : filtered.loot)
    {
        view.loot.push_back(QuantizeLoot(loot, minimum, maximum));
    }
    return view;
}

[[nodiscard]] SnapshotPayload BuildQuantizedKeyframe(
    const std::uint32_t snapshotId,
    const QuantizedView& view)
{
    SnapshotPayload payload;
    payload.header = {
        SnapshotPayloadKind::Keyframe,
        SnapshotValueEncoding::Quantized,
        0U,
        snapshotId};
    payload.global = view.global;
    payload.actorValues = view.actors;
    payload.lootValues = view.loot;
    return payload;
}

[[nodiscard]] QuantizedGlobalDelta BuildGlobalDelta(
    const QuantizedGlobalValue& base,
    const QuantizedGlobalValue& current)
{
    QuantizedGlobalDelta delta;
    if (base.phase != current.phase)
    {
        delta.fields = delta.fields | GlobalField::Phase;
        delta.phase = current.phase;
    }
    if (base.safeZoneStage != current.safeZoneStage
        || base.safeZoneCenter != current.safeZoneCenter
        || base.safeZoneRadius != current.safeZoneRadius)
    {
        delta.fields = delta.fields | GlobalField::SafeZone;
        delta.safeZoneStage = current.safeZoneStage;
        delta.safeZoneCenter = current.safeZoneCenter;
        delta.safeZoneRadius = current.safeZoneRadius;
    }
    if (base.aliveContenders != current.aliveContenders)
    {
        delta.fields = delta.fields | GlobalField::AliveContenders;
        delta.aliveContenders = current.aliveContenders;
    }
    if (base.hasResult != current.hasResult
        || (current.hasResult && base.result != current.result))
    {
        delta.fields = delta.fields | GlobalField::Result;
        delta.hasResult = current.hasResult;
        if (current.hasResult)
        {
            delta.result = current.result;
        }
    }
    if (base.eventChecksum != current.eventChecksum)
    {
        delta.fields = delta.fields | GlobalField::EventChecksum;
        delta.eventChecksum = current.eventChecksum;
    }
    return delta;
}

[[nodiscard]] std::optional<QuantizedActorDelta> BuildActorDelta(
    const QuantizedActorValue& base,
    const QuantizedActorValue& current)
{
    if (base.role != current.role
        || base.neutralArchetype != current.neutralArchetype)
    {
        throw std::logic_error{"actor immutable replication field changed"};
    }

    QuantizedActorDelta delta;
    delta.id = current.id;
    if (base.position != current.position)
    {
        delta.fields = delta.fields | ActorField::Position;
        delta.position = current.position;
    }
    if (base.health != current.health || base.alive != current.alive)
    {
        delta.fields = delta.fields | ActorField::HealthAlive;
        delta.health = current.health;
        delta.alive = current.alive;
    }
    if (base.weapon != current.weapon
        || base.cooldownTicksRemaining != current.cooldownTicksRemaining)
    {
        delta.fields = delta.fields | ActorField::WeaponCooldown;
        delta.weapon = current.weapon;
        delta.cooldownTicksRemaining = current.cooldownTicksRemaining;
    }
    if (base.eliminations != current.eliminations)
    {
        delta.fields = delta.fields | ActorField::Eliminations;
        delta.eliminations = current.eliminations;
    }
    if (delta.fields == ActorField::None)
    {
        return std::nullopt;
    }
    return delta;
}

[[nodiscard]] std::optional<QuantizedLootDelta> BuildLootDelta(
    const QuantizedLootValue& base,
    const QuantizedLootValue& current)
{
    if (base.type != current.type || base.position != current.position)
    {
        throw std::logic_error{"loot immutable replication field changed"};
    }
    if (base.active == current.active)
    {
        return std::nullopt;
    }
    return QuantizedLootDelta{current.id, LootField::Active, current.active};
}

[[nodiscard]] SnapshotPayload BuildDeltaPayload(
    const std::uint32_t baseSnapshotId,
    const std::uint32_t snapshotId,
    const QuantizedView& base,
    const QuantizedView& current)
{
    SnapshotPayload payload;
    payload.header = {
        SnapshotPayloadKind::Delta,
        SnapshotValueEncoding::Quantized,
        baseSnapshotId,
        snapshotId};
    payload.globalDelta = BuildGlobalDelta(base.global, current.global);

    std::size_t baseIndex = 0U;
    std::size_t currentIndex = 0U;
    while (baseIndex < base.actors.size()
           || currentIndex < current.actors.size())
    {
        if (currentIndex == current.actors.size()
            || (baseIndex < base.actors.size()
                && base.actors[baseIndex].id
                    < current.actors[currentIndex].id))
        {
            payload.removedActors.push_back(base.actors[baseIndex].id);
            ++baseIndex;
            continue;
        }
        if (baseIndex == base.actors.size()
            || current.actors[currentIndex].id
                < base.actors[baseIndex].id)
        {
            payload.actorValues.push_back(current.actors[currentIndex]);
            ++currentIndex;
            continue;
        }

        const auto delta = BuildActorDelta(
            base.actors[baseIndex],
            current.actors[currentIndex]);
        if (delta.has_value())
        {
            payload.actorDeltas.push_back(*delta);
        }
        ++baseIndex;
        ++currentIndex;
    }

    baseIndex = 0U;
    currentIndex = 0U;
    while (baseIndex < base.loot.size()
           || currentIndex < current.loot.size())
    {
        if (currentIndex == current.loot.size()
            || (baseIndex < base.loot.size()
                && base.loot[baseIndex].id < current.loot[currentIndex].id))
        {
            payload.removedLoot.push_back(base.loot[baseIndex].id);
            ++baseIndex;
            continue;
        }
        if (baseIndex == base.loot.size()
            || current.loot[currentIndex].id < base.loot[baseIndex].id)
        {
            payload.lootValues.push_back(current.loot[currentIndex]);
            ++currentIndex;
            continue;
        }

        const auto delta = BuildLootDelta(
            base.loot[baseIndex],
            current.loot[currentIndex]);
        if (delta.has_value())
        {
            payload.lootDeltas.push_back(*delta);
        }
        ++baseIndex;
        ++currentIndex;
    }
    return payload;
}
} // namespace

struct SnapshotReplicator::Impl
{
    struct Baseline
    {
        std::uint32_t snapshotId = 0U;
        QuantizedView view;
    };

    struct Recipient
    {
        EntityId controlledActor;
        VisibleSet visible;
        std::uint32_t issuedHighWatermark = 0U;
        std::uint32_t acknowledgedSnapshotId = 0U;
        std::uint32_t buildOrdinal = 0U;
        std::uint32_t lastKeyframeOrdinal = 0U;
        bool keyframeRequested = false;
        std::deque<Baseline> baselines;
    };

    Impl(const ArenaMapDefinition& arena, ReplicationConfig requestedConfig)
        : config{ValidateConfig(std::move(requestedConfig))},
          bounds{ArenaBounds(arena)},
          grid{bounds, config.cellSize}
    {
        minimum = bounds.Minimum();
        maximum = bounds.Maximum();
        radiusMaximum = std::max(
            maximum.x - minimum.x,
            maximum.z - minimum.z)
            * 0.5F;
    }

    [[nodiscard]] Recipient& RecipientFor(const PlayerId player)
    {
        const auto found = recipients.find(player);
        if (found == recipients.end())
        {
            throw std::out_of_range{"replication recipient is not registered"};
        }
        return found->second;
    }

    [[nodiscard]] const InterestGrid& PrepareGrid(
        const std::uint32_t snapshotId,
        const GameSnapshot& world,
        std::optional<InterestGrid>& candidate)
    {
        if (gridSnapshotId.has_value() && snapshotId == *gridSnapshotId)
        {
            if (!gridWorld.has_value() || *gridWorld != world)
            {
                throw std::invalid_argument{
                    "same snapshot ID has a different world"};
            }
            return grid;
        }
        if (gridSnapshotId.has_value() && snapshotId < *gridSnapshotId)
        {
            throw std::invalid_argument{"snapshot ID moved backward"};
        }

        candidate.emplace(bounds, config.cellSize);
        candidate->Rebuild(world);
        return *candidate;
    }

    void CommitGrid(
        const std::uint32_t snapshotId,
        const GameSnapshot& world,
        std::optional<InterestGrid>& candidate)
    {
        if (!candidate.has_value())
        {
            return;
        }
        grid = std::move(*candidate);
        gridSnapshotId = snapshotId;
        gridWorld = world;
    }

    void StoreBaseline(
        Recipient& recipient,
        const std::uint32_t snapshotId,
        const QuantizedView& view)
    {
        recipient.baselines.push_back(Baseline{snapshotId, view});
        while (recipient.baselines.size()
               > config.maximumBaselinesPerRecipient)
        {
            const bool frontIsAcknowledged =
                recipient.acknowledgedSnapshotId != 0U
                && recipient.baselines.front().snapshotId
                    == recipient.acknowledgedSnapshotId;
            if (frontIsAcknowledged
                && config.maximumBaselinesPerRecipient > 1U)
            {
                recipient.baselines.erase(
                    std::next(recipient.baselines.begin()));
            }
            else
            {
                recipient.baselines.pop_front();
            }
        }
    }

    [[nodiscard]] const Baseline* FindBaseline(
        const Recipient& recipient,
        const std::uint32_t snapshotId) const
    {
        const auto found = std::find_if(
            recipient.baselines.begin(),
            recipient.baselines.end(),
            [snapshotId](const Baseline& baseline) {
                return baseline.snapshotId == snapshotId;
            });
        return found == recipient.baselines.end() ? nullptr : &*found;
    }

    ReplicationConfig config;
    Aabb2 bounds;
    Vec2 minimum;
    Vec2 maximum;
    float radiusMaximum = 0.0F;
    InterestGrid grid;
    std::optional<std::uint32_t> gridSnapshotId;
    std::optional<GameSnapshot> gridWorld;
    std::map<PlayerId, Recipient> recipients;
};

SnapshotReplicator::SnapshotReplicator(
    const ArenaMapDefinition& arena,
    ReplicationConfig config)
    : impl_{std::make_unique<Impl>(arena, std::move(config))}
{
}

SnapshotReplicator::~SnapshotReplicator() = default;
SnapshotReplicator::SnapshotReplicator(SnapshotReplicator&&) noexcept = default;
SnapshotReplicator& SnapshotReplicator::operator=(
    SnapshotReplicator&&) noexcept = default;

void SnapshotReplicator::RegisterRecipient(
    const PlayerId player,
    const EntityId controlledActor)
{
    if (impl_->recipients.contains(player)
        || std::any_of(
            impl_->recipients.begin(),
            impl_->recipients.end(),
            [controlledActor](const auto& entry) {
                return entry.second.controlledActor == controlledActor;
            }))
    {
        throw std::invalid_argument{"replication recipient is duplicate"};
    }
    impl_->recipients.emplace(
        player,
        Impl::Recipient{
            controlledActor,
            VisibleSet{},
            0U,
            0U,
            0U,
            0U,
            false,
            {}});
}

bool SnapshotReplicator::AcceptAcknowledgement(
    const PlayerId player,
    const std::uint32_t snapshotId)
{
    Impl::Recipient& recipient = impl_->RecipientFor(player);
    if (snapshotId == 0U
        || snapshotId > recipient.issuedHighWatermark)
    {
        return false;
    }
    if (snapshotId > recipient.acknowledgedSnapshotId)
    {
        recipient.acknowledgedSnapshotId = snapshotId;
        while (!recipient.baselines.empty()
               && recipient.baselines.front().snapshotId < snapshotId)
        {
            recipient.baselines.pop_front();
        }
    }
    return true;
}

void SnapshotReplicator::RequestKeyframe(const PlayerId player)
{
    impl_->RecipientFor(player).keyframeRequested = true;
}

ReplicationBuild SnapshotReplicator::Build(
    const PlayerId player,
    const std::uint32_t snapshotId,
    const GameSnapshot& world)
{
    Impl::Recipient& recipient = impl_->RecipientFor(player);
    if (snapshotId == 0U
        || snapshotId <= recipient.issuedHighWatermark)
    {
        throw std::invalid_argument{"snapshot ID is not increasing"};
    }

    ReplicationBuild build;
    if (impl_->config.mode == ReplicationMode::FullState)
    {
        build.payload.header = {
            SnapshotPayloadKind::FullState,
            SnapshotValueEncoding::FullPrecision,
            0U,
            snapshotId};
        build.payload.fullPrecision = world;
        build.visibleActorCount = static_cast<std::uint32_t>(
            world.actors.size());
        build.visibleLootCount = static_cast<std::uint32_t>(
            world.loot.size());
        build.encodedPayload = EncodeSnapshotPayload(build.payload);
        recipient.issuedHighWatermark = snapshotId;
        ++recipient.buildOrdinal;
        recipient.keyframeRequested = false;
        return build;
    }

    if (!HasCanonicalWorldIds(world))
    {
        throw std::invalid_argument{"interest world IDs are not canonical"};
    }

    const NetworkActorSnapshot* controlledActor = FindActor(
        world,
        recipient.controlledActor);
    if (controlledActor == nullptr)
    {
        throw std::logic_error{"controlled actor is missing from world"};
    }

    std::optional<InterestGrid> candidateGrid;
    const InterestGrid& queryGrid = impl_->PrepareGrid(
        snapshotId,
        world,
        candidateGrid);
    const VisibleSet visible = queryGrid.UpdateVisibility(
        recipient.visible,
        controlledActor->position,
        impl_->config.enterRadius,
        impl_->config.leaveRadius);
    if (!Contains(visible.actors, recipient.controlledActor))
    {
        throw std::logic_error{"controlled actor left its interest set"};
    }

    const GameSnapshot filtered = FilterWorld(world, visible);
    std::optional<QuantizedView> currentView;
    if (impl_->config.mode == ReplicationMode::InterestFullPrecision)
    {
        build.payload.header = {
            SnapshotPayloadKind::Keyframe,
            SnapshotValueEncoding::FullPrecision,
            0U,
            snapshotId};
        build.payload.fullPrecision = filtered;
    }
    else
    {
        currentView = QuantizeView(
            filtered,
            impl_->minimum,
            impl_->maximum,
            impl_->radiusMaximum);
        if (impl_->config.mode == ReplicationMode::InterestDelta)
        {
            const Impl::Baseline* acknowledgedBaseline =
                impl_->FindBaseline(
                    recipient,
                    recipient.acknowledgedSnapshotId);
            const std::uint32_t nextOrdinal = recipient.buildOrdinal + 1U;
            const bool periodicKeyframe =
                recipient.lastKeyframeOrdinal == 0U
                || nextOrdinal - recipient.lastKeyframeOrdinal
                    >= impl_->config.keyframeIntervalSnapshots;
            const bool missingAcknowledgedBaseline =
                recipient.acknowledgedSnapshotId == 0U
                || acknowledgedBaseline == nullptr;
            const bool makeKeyframe = recipient.keyframeRequested
                || periodicKeyframe
                || missingAcknowledgedBaseline;
            build.fallbackKeyframe =
                recipient.acknowledgedSnapshotId != 0U
                && acknowledgedBaseline == nullptr;
            if (makeKeyframe)
            {
                build.payload = BuildQuantizedKeyframe(
                    snapshotId,
                    *currentView);
                build.keyframe = true;
            }
            else
            {
                build.payload = BuildDeltaPayload(
                    recipient.acknowledgedSnapshotId,
                    snapshotId,
                    acknowledgedBaseline->view,
                    *currentView);
            }
        }
        else
        {
            build.payload = BuildQuantizedKeyframe(
                snapshotId,
                *currentView);
            build.keyframe = true;
        }
    }
    build.visibleActorCount = static_cast<std::uint32_t>(
        visible.actors.size());
    build.visibleLootCount = static_cast<std::uint32_t>(
        visible.loot.size());
    if (impl_->config.mode == ReplicationMode::InterestFullPrecision)
    {
        build.keyframe = true;
    }

    build.encodedPayload = EncodeSnapshotPayload(build.payload);
    impl_->CommitGrid(snapshotId, world, candidateGrid);
    if (currentView.has_value())
    {
        impl_->StoreBaseline(recipient, snapshotId, *currentView);
    }
    recipient.visible = visible;
    recipient.issuedHighWatermark = snapshotId;
    ++recipient.buildOrdinal;
    if (build.keyframe)
    {
        recipient.lastKeyframeOrdinal = recipient.buildOrdinal;
    }
    recipient.keyframeRequested = false;
    return build;
}

void SnapshotReplicator::RemoveRecipient(const PlayerId player)
{
    impl_->recipients.erase(player);
}
} // namespace dxa::game_server
