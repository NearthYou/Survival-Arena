#include <dxa/protocol/ReplicationSnapshotCodec.hpp>

#include <dxa/protocol/ByteCodec.hpp>
#include <dxa/protocol/GameSnapshotCodec.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dxa::protocol
{
namespace
{
enum class CanonicalOrder
{
    Canonical,
    Duplicate,
    OutOfOrder
};

template <typename Value, typename Key>
[[nodiscard]] CanonicalOrder CheckCanonicalOrder(
    const std::vector<Value>& values,
    Key key)
{
    for (std::size_t index = 1U; index < values.size(); ++index)
    {
        const auto previous = key(values[index - 1U]);
        const auto current = key(values[index]);
        if (current == previous)
        {
            return CanonicalOrder::Duplicate;
        }
        if (current < previous)
        {
            return CanonicalOrder::OutOfOrder;
        }
    }
    return CanonicalOrder::Canonical;
}

[[nodiscard]] bool IsValidPayloadKind(
    const SnapshotPayloadKind kind) noexcept
{
    switch (kind)
    {
    case SnapshotPayloadKind::FullState:
    case SnapshotPayloadKind::Keyframe:
    case SnapshotPayloadKind::Delta:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidValueEncoding(
    const SnapshotValueEncoding encoding) noexcept
{
    switch (encoding)
    {
    case SnapshotValueEncoding::FullPrecision:
    case SnapshotValueEncoding::Quantized:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidRole(const NetworkActorRole role) noexcept
{
    switch (role)
    {
    case NetworkActorRole::Contender:
    case NetworkActorRole::Neutral:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidArchetype(
    const NetworkNeutralArchetype archetype) noexcept
{
    switch (archetype)
    {
    case NetworkNeutralArchetype::None:
    case NetworkNeutralArchetype::Melee:
    case NetworkNeutralArchetype::Ranged:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidWeapon(const NetworkWeaponType weapon) noexcept
{
    switch (weapon)
    {
    case NetworkWeaponType::Blade:
    case NetworkWeaponType::Rifle:
    case NetworkWeaponType::ArcPulse:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidLootType(const NetworkLootType type) noexcept
{
    switch (type)
    {
    case NetworkLootType::Rifle:
    case NetworkLootType::ArcPulse:
    case NetworkLootType::MedKit:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidPhase(const NetworkMatchPhase phase) noexcept
{
    switch (phase)
    {
    case NetworkMatchPhase::Waiting:
    case NetworkMatchPhase::Running:
    case NetworkMatchPhase::SuddenDeath:
    case NetworkMatchPhase::Finished:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidSafeZoneStage(
    const NetworkSafeZoneStage stage) noexcept
{
    switch (stage)
    {
    case NetworkSafeZoneStage::Stage1:
    case NetworkSafeZoneStage::Stage2:
    case NetworkSafeZoneStage::Stage3:
    case NetworkSafeZoneStage::Stage4:
    case NetworkSafeZoneStage::SuddenDeath:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidEndReason(
    const NetworkMatchEndReason reason) noexcept
{
    switch (reason)
    {
    case NetworkMatchEndReason::LastSurvivor:
    case NetworkMatchEndReason::TimeLimit:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsFinite(const NetworkVec2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.z);
}

[[nodiscard]] bool IsValidActorIdentity(
    const NetworkActorRole role,
    const NetworkNeutralArchetype archetype) noexcept
{
    if (!IsValidRole(role) || !IsValidArchetype(archetype))
    {
        return false;
    }
    if (role == NetworkActorRole::Contender)
    {
        return archetype == NetworkNeutralArchetype::None;
    }
    return archetype == NetworkNeutralArchetype::Melee
        || archetype == NetworkNeutralArchetype::Ranged;
}

[[nodiscard]] bool IsValidResultState(
    const NetworkMatchPhase phase,
    const std::uint32_t aliveContenders,
    const bool hasResult,
    const NetworkMatchResult& result) noexcept
{
    const bool finished = phase == NetworkMatchPhase::Finished;
    if (hasResult != finished)
    {
        return false;
    }
    return !hasResult
        || (aliveContenders == 1U && IsValidEndReason(result.reason));
}

[[nodiscard]] bool IsValidHeader(
    const SnapshotPayloadHeader& header) noexcept
{
    if (!IsValidPayloadKind(header.kind)
        || !IsValidValueEncoding(header.valueEncoding)
        || header.payloadSnapshotId == 0U)
    {
        return false;
    }

    switch (header.kind)
    {
    case SnapshotPayloadKind::FullState:
        return header.valueEncoding == SnapshotValueEncoding::FullPrecision
            && header.baseSnapshotId == 0U;
    case SnapshotPayloadKind::Keyframe:
        return header.baseSnapshotId == 0U;
    case SnapshotPayloadKind::Delta:
        return header.valueEncoding == SnapshotValueEncoding::Quantized
            && header.baseSnapshotId > 0U
            && header.baseSnapshotId < header.payloadSnapshotId;
    }
    return false;
}

[[nodiscard]] bool IsValidPrecisionActor(
    const NetworkActorSnapshot& actor) noexcept
{
    return IsValidActorIdentity(actor.role, actor.neutralArchetype)
        && IsValidWeapon(actor.weapon)
        && IsFinite(actor.position)
        && actor.health >= 0
        && actor.health <= 100
        && actor.alive == (actor.health > 0);
}

[[nodiscard]] bool IsValidQuantizedActor(
    const QuantizedActorValue& actor) noexcept
{
    return IsValidActorIdentity(actor.role, actor.neutralArchetype)
        && IsValidWeapon(actor.weapon)
        && actor.health <= 100U
        && actor.alive == (actor.health > 0U);
}

[[nodiscard]] bool IsValidQuantizedActorDelta(
    const QuantizedActorDelta& actor) noexcept
{
    if (actor.fields == ActorField::None
        || !IsValidActorFieldMask(actor.fields))
    {
        return false;
    }

    const auto hasField = [&actor](const ActorField field) {
        return (detail::ToUnderlying(actor.fields)
                & detail::ToUnderlying(field))
            != 0U;
    };
    const QuantizedActorDelta defaults;
    if ((!hasField(ActorField::Position)
         && actor.position != defaults.position)
        || (!hasField(ActorField::HealthAlive)
            && (actor.health != defaults.health
                || actor.alive != defaults.alive))
        || (!hasField(ActorField::WeaponCooldown)
            && (actor.weapon != defaults.weapon
                || actor.cooldownTicksRemaining
                    != defaults.cooldownTicksRemaining))
        || (!hasField(ActorField::Eliminations)
            && actor.eliminations != defaults.eliminations))
    {
        return false;
    }
    if (hasField(ActorField::HealthAlive)
        && (actor.health > 100U
            || actor.alive != (actor.health > 0U)))
    {
        return false;
    }
    return !hasField(ActorField::WeaponCooldown)
        || IsValidWeapon(actor.weapon);
}

[[nodiscard]] bool IsValidPrecisionGlobal(
    const GameSnapshot& snapshot) noexcept
{
    return IsValidPhase(snapshot.phase)
        && IsValidSafeZoneStage(snapshot.safeZoneStage)
        && IsFinite(snapshot.safeZoneCenter)
        && std::isfinite(snapshot.safeZoneRadius)
        && snapshot.safeZoneRadius >= 0.0F
        && snapshot.aliveContenders <= RoomCapacity
        && IsValidResultState(
            snapshot.phase,
            snapshot.aliveContenders,
            snapshot.hasResult,
            snapshot.result);
}

[[nodiscard]] bool IsValidQuantizedGlobal(
    const QuantizedGlobalValue& global) noexcept
{
    return IsValidPhase(global.phase)
        && IsValidSafeZoneStage(global.safeZoneStage)
        && global.aliveContenders <= RoomCapacity
        && IsValidResultState(
            global.phase,
            global.aliveContenders,
            global.hasResult,
            global.result);
}

[[nodiscard]] bool IsValidQuantizedGlobalDelta(
    const QuantizedGlobalDelta& global) noexcept
{
    if (!IsValidGlobalFieldMask(global.fields))
    {
        return false;
    }
    const auto hasField = [&global](const GlobalField field) {
        return (detail::ToUnderlying(global.fields)
                & detail::ToUnderlying(field))
            != 0U;
    };
    const QuantizedGlobalDelta defaults;
    if ((!hasField(GlobalField::Phase)
         && global.phase != defaults.phase)
        || (!hasField(GlobalField::SafeZone)
            && (global.safeZoneStage != defaults.safeZoneStage
                || global.safeZoneCenter != defaults.safeZoneCenter
                || global.safeZoneRadius != defaults.safeZoneRadius))
        || (!hasField(GlobalField::AliveContenders)
            && global.aliveContenders != defaults.aliveContenders)
        || (!hasField(GlobalField::Result)
            && (global.result != defaults.result
                || global.hasResult != defaults.hasResult))
        || (!hasField(GlobalField::EventChecksum)
            && global.eventChecksum != defaults.eventChecksum))
    {
        return false;
    }
    if (hasField(GlobalField::Phase) && !IsValidPhase(global.phase))
    {
        return false;
    }
    if (hasField(GlobalField::SafeZone)
        && !IsValidSafeZoneStage(global.safeZoneStage))
    {
        return false;
    }
    if (hasField(GlobalField::AliveContenders)
        && global.aliveContenders > RoomCapacity)
    {
        return false;
    }
    if (!hasField(GlobalField::Result))
    {
        return true;
    }
    if (global.hasResult && !IsValidEndReason(global.result.reason))
    {
        return false;
    }
    if (!global.hasResult && global.result != NetworkMatchResult{})
    {
        return false;
    }
    if (hasField(GlobalField::Phase)
        && global.hasResult
            != (global.phase == NetworkMatchPhase::Finished))
    {
        return false;
    }
    return !global.hasResult
        || !hasField(GlobalField::AliveContenders)
        || global.aliveContenders == 1U;
}

template <typename Value, typename Key>
void RequireCanonical(
    const std::vector<Value>& values,
    Key key)
{
    if (CheckCanonicalOrder(values, key) != CanonicalOrder::Canonical)
    {
        throw std::invalid_argument{"snapshot IDs are not canonical"};
    }
}

[[nodiscard]] bool HasDuplicateActorCategory(
    const SnapshotPayload& payload)
{
    std::vector<std::uint32_t> ids;
    ids.reserve(
        payload.actorValues.size()
        + payload.actorDeltas.size()
        + payload.removedActors.size());
    for (const QuantizedActorValue& actor : payload.actorValues)
    {
        ids.push_back(actor.id.value);
    }
    for (const QuantizedActorDelta& actor : payload.actorDeltas)
    {
        ids.push_back(actor.id.value);
    }
    for (const EntityId actor : payload.removedActors)
    {
        ids.push_back(actor.value);
    }
    std::sort(ids.begin(), ids.end());
    return std::adjacent_find(ids.begin(), ids.end()) != ids.end();
}

[[nodiscard]] bool HasDuplicateLootCategory(
    const SnapshotPayload& payload)
{
    std::vector<std::uint32_t> ids;
    ids.reserve(
        payload.lootValues.size()
        + payload.lootDeltas.size()
        + payload.removedLoot.size());
    for (const QuantizedLootValue& loot : payload.lootValues)
    {
        ids.push_back(loot.id);
    }
    for (const QuantizedLootDelta& loot : payload.lootDeltas)
    {
        ids.push_back(loot.id);
    }
    ids.insert(
        ids.end(),
        payload.removedLoot.begin(),
        payload.removedLoot.end());
    std::sort(ids.begin(), ids.end());
    return std::adjacent_find(ids.begin(), ids.end()) != ids.end();
}

[[nodiscard]] bool HasDefaultPrecisionBody(
    const SnapshotPayload& payload)
{
    return payload.fullPrecision == GameSnapshot{};
}

[[nodiscard]] bool HasDefaultQuantizedGlobal(
    const SnapshotPayload& payload)
{
    return payload.global == QuantizedGlobalValue{};
}

[[nodiscard]] bool HasDefaultGlobalDelta(
    const SnapshotPayload& payload)
{
    return payload.globalDelta == QuantizedGlobalDelta{};
}

void ValidatePrecisionKeyframe(const SnapshotPayload& payload)
{
    const GameSnapshot& snapshot = payload.fullPrecision;
    if (!IsValidPrecisionGlobal(snapshot)
        || snapshot.actors.size() > MaxSnapshotActors
        || snapshot.loot.size() > MaxSnapshotLoot
        || !std::all_of(
            snapshot.actors.begin(),
            snapshot.actors.end(),
            IsValidPrecisionActor)
        || !std::all_of(
            snapshot.loot.begin(),
            snapshot.loot.end(),
            [](const NetworkLootSnapshot& loot) {
                return IsValidLootType(loot.type)
                    && IsFinite(loot.position);
            }))
    {
        throw std::invalid_argument{"full-precision keyframe is invalid"};
    }
    RequireCanonical(
        snapshot.actors,
        [](const NetworkActorSnapshot& actor) { return actor.id; });
    RequireCanonical(
        snapshot.loot,
        [](const NetworkLootSnapshot& loot) { return loot.id; });
}

void ValidateQuantizedKeyframe(const SnapshotPayload& payload)
{
    if (!IsValidQuantizedGlobal(payload.global)
        || payload.actorValues.size() > MaxSnapshotActors
        || payload.lootValues.size() > MaxSnapshotLoot
        || !std::all_of(
            payload.actorValues.begin(),
            payload.actorValues.end(),
            IsValidQuantizedActor)
        || !std::all_of(
            payload.lootValues.begin(),
            payload.lootValues.end(),
            [](const QuantizedLootValue& loot) {
                return IsValidLootType(loot.type);
            }))
    {
        throw std::invalid_argument{"quantized keyframe is invalid"};
    }
    RequireCanonical(
        payload.actorValues,
        [](const QuantizedActorValue& actor) { return actor.id; });
    RequireCanonical(
        payload.lootValues,
        [](const QuantizedLootValue& loot) { return loot.id; });
}

void ValidateDelta(const SnapshotPayload& payload)
{
    const std::size_t actorCount = payload.actorValues.size()
        + payload.actorDeltas.size()
        + payload.removedActors.size();
    const std::size_t lootCount = payload.lootValues.size()
        + payload.lootDeltas.size()
        + payload.removedLoot.size();
    if (actorCount > MaxSnapshotActors || lootCount > MaxSnapshotLoot)
    {
        throw std::length_error{"delta record count exceeds snapshot bounds"};
    }
    if (!IsValidQuantizedGlobalDelta(payload.globalDelta)
        || !std::all_of(
            payload.actorValues.begin(),
            payload.actorValues.end(),
            IsValidQuantizedActor)
        || !std::all_of(
            payload.actorDeltas.begin(),
            payload.actorDeltas.end(),
            IsValidQuantizedActorDelta)
        || !std::all_of(
            payload.lootValues.begin(),
            payload.lootValues.end(),
            [](const QuantizedLootValue& loot) {
                return IsValidLootType(loot.type);
            })
        || !std::all_of(
            payload.lootDeltas.begin(),
            payload.lootDeltas.end(),
            [](const QuantizedLootDelta& loot) {
                return loot.fields == LootField::Active;
            }))
    {
        throw std::invalid_argument{"delta payload is invalid"};
    }

    RequireCanonical(
        payload.actorValues,
        [](const QuantizedActorValue& actor) { return actor.id; });
    RequireCanonical(
        payload.actorDeltas,
        [](const QuantizedActorDelta& actor) { return actor.id; });
    RequireCanonical(
        payload.removedActors,
        [](const EntityId actor) { return actor; });
    RequireCanonical(
        payload.lootValues,
        [](const QuantizedLootValue& loot) { return loot.id; });
    RequireCanonical(
        payload.lootDeltas,
        [](const QuantizedLootDelta& loot) { return loot.id; });
    RequireCanonical(
        payload.removedLoot,
        [](const std::uint32_t loot) { return loot; });
    if (HasDuplicateActorCategory(payload)
        || HasDuplicateLootCategory(payload))
    {
        throw std::invalid_argument{"delta entity appears in multiple records"};
    }
}

void ValidatePayload(const SnapshotPayload& payload)
{
    if (!IsValidHeader(payload.header))
    {
        throw std::invalid_argument{"snapshot payload header is invalid"};
    }

    const bool hasDeltaRecords = !payload.actorDeltas.empty()
        || !payload.lootDeltas.empty()
        || !payload.removedActors.empty()
        || !payload.removedLoot.empty()
        || !HasDefaultGlobalDelta(payload);
    switch (payload.header.kind)
    {
    case SnapshotPayloadKind::FullState:
        if (payload.header.valueEncoding != SnapshotValueEncoding::FullPrecision
            || !payload.actorValues.empty()
            || !payload.lootValues.empty()
            || hasDeltaRecords
            || !HasDefaultQuantizedGlobal(payload))
        {
            throw std::invalid_argument{"full-state payload has mixed bodies"};
        }
        return;
    case SnapshotPayloadKind::Keyframe:
        if (hasDeltaRecords)
        {
            throw std::invalid_argument{"keyframe payload has delta records"};
        }
        if (payload.header.valueEncoding
            == SnapshotValueEncoding::FullPrecision)
        {
            if (!payload.actorValues.empty()
                || !payload.lootValues.empty()
                || !HasDefaultQuantizedGlobal(payload))
            {
                throw std::invalid_argument{
                    "full-precision keyframe has mixed bodies"};
            }
            ValidatePrecisionKeyframe(payload);
            return;
        }
        if (!HasDefaultPrecisionBody(payload))
        {
            throw std::invalid_argument{"quantized keyframe has mixed bodies"};
        }
        ValidateQuantizedKeyframe(payload);
        return;
    case SnapshotPayloadKind::Delta:
        if (!HasDefaultPrecisionBody(payload)
            || !HasDefaultQuantizedGlobal(payload))
        {
            throw std::invalid_argument{"delta payload has mixed bodies"};
        }
        ValidateDelta(payload);
        return;
    }
    throw std::invalid_argument{"snapshot payload kind is invalid"};
}

void WriteHeader(ByteWriter& writer, const SnapshotPayloadHeader& header)
{
    writer.WriteU8(static_cast<std::uint8_t>(header.kind));
    writer.WriteU8(static_cast<std::uint8_t>(header.valueEncoding));
    writer.WriteU32(header.baseSnapshotId);
    writer.WriteU32(header.payloadSnapshotId);
}

void WriteResult(ByteWriter& writer, const NetworkMatchResult& result)
{
    writer.WriteU32(result.winner.value);
    writer.WriteU8(static_cast<std::uint8_t>(result.reason));
    writer.WriteU32(result.finishedTick);
}

void WritePrecisionActor(
    ByteWriter& writer,
    const NetworkActorSnapshot& actor)
{
    writer.WriteU32(actor.id.value);
    writer.WriteU8(static_cast<std::uint8_t>(actor.role));
    writer.WriteU8(static_cast<std::uint8_t>(actor.neutralArchetype));
    writer.WriteF32(actor.position.x);
    writer.WriteF32(actor.position.z);
    writer.WriteU32(std::bit_cast<std::uint32_t>(actor.health));
    writer.WriteU8(actor.alive ? 1U : 0U);
    writer.WriteU8(static_cast<std::uint8_t>(actor.weapon));
    writer.WriteU32(actor.cooldownTicksRemaining);
    writer.WriteU32(actor.eliminations);
}

void WritePrecisionLoot(
    ByteWriter& writer,
    const NetworkLootSnapshot& loot)
{
    writer.WriteU32(loot.id);
    writer.WriteU8(static_cast<std::uint8_t>(loot.type));
    writer.WriteF32(loot.position.x);
    writer.WriteF32(loot.position.z);
    writer.WriteU8(loot.active ? 1U : 0U);
}

void WritePrecisionKeyframe(
    ByteWriter& writer,
    const GameSnapshot& snapshot)
{
    writer.WriteU8(static_cast<std::uint8_t>(snapshot.phase));
    writer.WriteU8(static_cast<std::uint8_t>(snapshot.safeZoneStage));
    writer.WriteF32(snapshot.safeZoneCenter.x);
    writer.WriteF32(snapshot.safeZoneCenter.z);
    writer.WriteF32(snapshot.safeZoneRadius);
    writer.WriteU32(snapshot.aliveContenders);
    writer.WriteU16(static_cast<std::uint16_t>(snapshot.actors.size()));
    for (const NetworkActorSnapshot& actor : snapshot.actors)
    {
        WritePrecisionActor(writer, actor);
    }
    writer.WriteU16(static_cast<std::uint16_t>(snapshot.loot.size()));
    for (const NetworkLootSnapshot& loot : snapshot.loot)
    {
        WritePrecisionLoot(writer, loot);
    }
    writer.WriteU8(snapshot.hasResult ? 1U : 0U);
    if (snapshot.hasResult)
    {
        WriteResult(writer, snapshot.result);
    }
    writer.WriteU64(snapshot.eventChecksum);
}

void WriteQuantizedActor(
    ByteWriter& writer,
    const QuantizedActorValue& actor)
{
    writer.WriteU32(actor.id.value);
    writer.WriteU8(static_cast<std::uint8_t>(actor.role));
    writer.WriteU8(static_cast<std::uint8_t>(actor.neutralArchetype));
    writer.WriteU16(actor.position.x);
    writer.WriteU16(actor.position.z);
    writer.WriteU8(actor.health);
    writer.WriteU8(actor.alive ? 1U : 0U);
    writer.WriteU8(static_cast<std::uint8_t>(actor.weapon));
    writer.WriteU16(actor.cooldownTicksRemaining);
    writer.WriteU8(actor.eliminations);
}

void WriteQuantizedLoot(
    ByteWriter& writer,
    const QuantizedLootValue& loot)
{
    writer.WriteU32(loot.id);
    writer.WriteU8(static_cast<std::uint8_t>(loot.type));
    writer.WriteU16(loot.position.x);
    writer.WriteU16(loot.position.z);
    writer.WriteU8(loot.active ? 1U : 0U);
}

void WriteQuantizedGlobal(
    ByteWriter& writer,
    const QuantizedGlobalValue& global)
{
    writer.WriteU8(static_cast<std::uint8_t>(global.phase));
    writer.WriteU8(static_cast<std::uint8_t>(global.safeZoneStage));
    writer.WriteU16(global.safeZoneCenter.x);
    writer.WriteU16(global.safeZoneCenter.z);
    writer.WriteU16(global.safeZoneRadius);
    writer.WriteU8(global.aliveContenders);
    writer.WriteU8(global.hasResult ? 1U : 0U);
    if (global.hasResult)
    {
        WriteResult(writer, global.result);
    }
    writer.WriteU64(global.eventChecksum);
}

void WriteQuantizedKeyframe(
    ByteWriter& writer,
    const SnapshotPayload& payload)
{
    WriteQuantizedGlobal(writer, payload.global);
    writer.WriteU16(static_cast<std::uint16_t>(payload.actorValues.size()));
    for (const QuantizedActorValue& actor : payload.actorValues)
    {
        WriteQuantizedActor(writer, actor);
    }
    writer.WriteU16(static_cast<std::uint16_t>(payload.lootValues.size()));
    for (const QuantizedLootValue& loot : payload.lootValues)
    {
        WriteQuantizedLoot(writer, loot);
    }
}

template <typename Field>
[[nodiscard]] bool HasField(const Field fields, const Field field) noexcept
{
    return (detail::ToUnderlying(fields) & detail::ToUnderlying(field)) != 0U;
}

void WriteGlobalDelta(
    ByteWriter& writer,
    const QuantizedGlobalDelta& global)
{
    writer.WriteU8(detail::ToUnderlying(global.fields));
    if (HasField(global.fields, GlobalField::Phase))
    {
        writer.WriteU8(static_cast<std::uint8_t>(global.phase));
    }
    if (HasField(global.fields, GlobalField::SafeZone))
    {
        writer.WriteU8(static_cast<std::uint8_t>(global.safeZoneStage));
        writer.WriteU16(global.safeZoneCenter.x);
        writer.WriteU16(global.safeZoneCenter.z);
        writer.WriteU16(global.safeZoneRadius);
    }
    if (HasField(global.fields, GlobalField::AliveContenders))
    {
        writer.WriteU8(global.aliveContenders);
    }
    if (HasField(global.fields, GlobalField::Result))
    {
        writer.WriteU8(global.hasResult ? 1U : 0U);
        if (global.hasResult)
        {
            WriteResult(writer, global.result);
        }
    }
    if (HasField(global.fields, GlobalField::EventChecksum))
    {
        writer.WriteU64(global.eventChecksum);
    }
}

void WriteActorDelta(
    ByteWriter& writer,
    const QuantizedActorDelta& actor)
{
    writer.WriteU32(actor.id.value);
    writer.WriteU8(detail::ToUnderlying(actor.fields));
    if (HasField(actor.fields, ActorField::Position))
    {
        writer.WriteU16(actor.position.x);
        writer.WriteU16(actor.position.z);
    }
    if (HasField(actor.fields, ActorField::HealthAlive))
    {
        writer.WriteU8(actor.health);
        writer.WriteU8(actor.alive ? 1U : 0U);
    }
    if (HasField(actor.fields, ActorField::WeaponCooldown))
    {
        writer.WriteU8(static_cast<std::uint8_t>(actor.weapon));
        writer.WriteU16(actor.cooldownTicksRemaining);
    }
    if (HasField(actor.fields, ActorField::Eliminations))
    {
        writer.WriteU8(actor.eliminations);
    }
}

void WriteDelta(ByteWriter& writer, const SnapshotPayload& payload)
{
    WriteGlobalDelta(writer, payload.globalDelta);

    writer.WriteU16(static_cast<std::uint16_t>(payload.actorValues.size()));
    for (const QuantizedActorValue& actor : payload.actorValues)
    {
        WriteQuantizedActor(writer, actor);
    }
    writer.WriteU16(static_cast<std::uint16_t>(payload.actorDeltas.size()));
    for (const QuantizedActorDelta& actor : payload.actorDeltas)
    {
        WriteActorDelta(writer, actor);
    }
    writer.WriteU16(static_cast<std::uint16_t>(payload.removedActors.size()));
    for (const EntityId actor : payload.removedActors)
    {
        writer.WriteU32(actor.value);
    }

    writer.WriteU16(static_cast<std::uint16_t>(payload.lootValues.size()));
    for (const QuantizedLootValue& loot : payload.lootValues)
    {
        WriteQuantizedLoot(writer, loot);
    }
    writer.WriteU16(static_cast<std::uint16_t>(payload.lootDeltas.size()));
    for (const QuantizedLootDelta& loot : payload.lootDeltas)
    {
        writer.WriteU32(loot.id);
        writer.WriteU8(detail::ToUnderlying(loot.fields));
        writer.WriteU8(loot.active ? 1U : 0U);
    }
    writer.WriteU16(static_cast<std::uint16_t>(payload.removedLoot.size()));
    for (const std::uint32_t loot : payload.removedLoot)
    {
        writer.WriteU32(loot);
    }
}

[[nodiscard]] SnapshotPayloadDecodeResult Failure(
    const SnapshotPayloadDecodeError error)
{
    return {std::nullopt, error};
}

[[nodiscard]] SnapshotPayloadDecodeError ReaderFailure(
    const ByteReader& reader)
{
    switch (reader.Error())
    {
    case DecodeError::Truncated:
        return SnapshotPayloadDecodeError::Truncated;
    case DecodeError::InvalidValue:
        return SnapshotPayloadDecodeError::InvalidValue;
    case DecodeError::CountLimit:
        return SnapshotPayloadDecodeError::CountLimit;
    case DecodeError::TrailingBytes:
        return SnapshotPayloadDecodeError::TrailingBytes;
    case DecodeError::None:
        return SnapshotPayloadDecodeError::TrailingBytes;
    }
    return SnapshotPayloadDecodeError::InvalidValue;
}

[[nodiscard]] SnapshotPayloadDecodeError GameSnapshotFailure(
    const DecodeError error)
{
    switch (error)
    {
    case DecodeError::None:
        return SnapshotPayloadDecodeError::None;
    case DecodeError::Truncated:
        return SnapshotPayloadDecodeError::Truncated;
    case DecodeError::InvalidValue:
        return SnapshotPayloadDecodeError::InvalidValue;
    case DecodeError::CountLimit:
        return SnapshotPayloadDecodeError::CountLimit;
    case DecodeError::TrailingBytes:
        return SnapshotPayloadDecodeError::TrailingBytes;
    }
    return SnapshotPayloadDecodeError::InvalidValue;
}

void SetError(
    SnapshotPayloadDecodeError& error,
    const SnapshotPayloadDecodeError next) noexcept
{
    if (error == SnapshotPayloadDecodeError::None)
    {
        error = next;
    }
}

[[nodiscard]] bool ReadBool(
    ByteReader& reader,
    bool& value,
    SnapshotPayloadDecodeError& error)
{
    const auto encoded = reader.ReadU8();
    if (!encoded.has_value())
    {
        return false;
    }
    if (*encoded > 1U)
    {
        SetError(error, SnapshotPayloadDecodeError::InvalidValue);
        return false;
    }
    value = *encoded == 1U;
    return true;
}

[[nodiscard]] bool ReadResult(
    ByteReader& reader,
    NetworkMatchResult& result)
{
    const auto winner = reader.ReadU32();
    const auto reason = reader.ReadU8();
    const auto finishedTick = reader.ReadU32();
    if (!winner.has_value()
        || !reason.has_value()
        || !finishedTick.has_value())
    {
        return false;
    }
    result = {
        EntityId{*winner},
        static_cast<NetworkMatchEndReason>(*reason),
        *finishedTick};
    return true;
}

[[nodiscard]] bool ReadPrecisionActor(
    ByteReader& reader,
    NetworkActorSnapshot& actor,
    SnapshotPayloadDecodeError& error)
{
    const auto id = reader.ReadU32();
    const auto role = reader.ReadU8();
    const auto archetype = reader.ReadU8();
    const auto positionX = reader.ReadF32();
    const auto positionZ = reader.ReadF32();
    const auto health = reader.ReadU32();
    const auto alive = reader.ReadU8();
    const auto weapon = reader.ReadU8();
    const auto cooldown = reader.ReadU32();
    const auto eliminations = reader.ReadU32();
    if (!id.has_value()
        || !role.has_value()
        || !archetype.has_value()
        || !positionX.has_value()
        || !positionZ.has_value()
        || !health.has_value()
        || !alive.has_value()
        || !weapon.has_value()
        || !cooldown.has_value()
        || !eliminations.has_value())
    {
        return false;
    }
    if (*alive > 1U)
    {
        SetError(error, SnapshotPayloadDecodeError::InvalidValue);
        return false;
    }
    actor = {
        EntityId{*id},
        static_cast<NetworkActorRole>(*role),
        static_cast<NetworkNeutralArchetype>(*archetype),
        {*positionX, *positionZ},
        std::bit_cast<std::int32_t>(*health),
        *alive == 1U,
        static_cast<NetworkWeaponType>(*weapon),
        *cooldown,
        *eliminations};
    return true;
}

[[nodiscard]] bool ReadPrecisionLoot(
    ByteReader& reader,
    NetworkLootSnapshot& loot,
    SnapshotPayloadDecodeError& error)
{
    const auto id = reader.ReadU32();
    const auto type = reader.ReadU8();
    const auto positionX = reader.ReadF32();
    const auto positionZ = reader.ReadF32();
    const auto active = reader.ReadU8();
    if (!id.has_value()
        || !type.has_value()
        || !positionX.has_value()
        || !positionZ.has_value()
        || !active.has_value())
    {
        return false;
    }
    if (*active > 1U)
    {
        SetError(error, SnapshotPayloadDecodeError::InvalidValue);
        return false;
    }
    loot = {
        *id,
        static_cast<NetworkLootType>(*type),
        {*positionX, *positionZ},
        *active == 1U};
    return true;
}

template <typename Value, typename Reader>
[[nodiscard]] bool ReadBoundedVector(
    ByteReader& reader,
    std::vector<Value>& values,
    const std::size_t maximum,
    SnapshotPayloadDecodeError& error,
    Reader readValue)
{
    const auto count = reader.ReadU16();
    if (!count.has_value())
    {
        return false;
    }
    if (*count > maximum)
    {
        SetError(error, SnapshotPayloadDecodeError::CountLimit);
        return false;
    }
    values.reserve(*count);
    for (std::uint16_t index = 0U; index < *count; ++index)
    {
        Value value;
        if (!readValue(reader, value, error))
        {
            return false;
        }
        values.push_back(std::move(value));
    }
    return true;
}

[[nodiscard]] bool ReadPrecisionKeyframe(
    ByteReader& reader,
    GameSnapshot& snapshot,
    SnapshotPayloadDecodeError& error)
{
    const auto phase = reader.ReadU8();
    const auto stage = reader.ReadU8();
    const auto centerX = reader.ReadF32();
    const auto centerZ = reader.ReadF32();
    const auto radius = reader.ReadF32();
    const auto aliveContenders = reader.ReadU32();
    if (!phase.has_value()
        || !stage.has_value()
        || !centerX.has_value()
        || !centerZ.has_value()
        || !radius.has_value()
        || !aliveContenders.has_value())
    {
        return false;
    }
    snapshot.phase = static_cast<NetworkMatchPhase>(*phase);
    snapshot.safeZoneStage = static_cast<NetworkSafeZoneStage>(*stage);
    snapshot.safeZoneCenter = {*centerX, *centerZ};
    snapshot.safeZoneRadius = *radius;
    snapshot.aliveContenders = *aliveContenders;

    if (!ReadBoundedVector(
            reader,
            snapshot.actors,
            MaxSnapshotActors,
            error,
            ReadPrecisionActor)
        || !ReadBoundedVector(
            reader,
            snapshot.loot,
            MaxSnapshotLoot,
            error,
            ReadPrecisionLoot)
        || !ReadBool(reader, snapshot.hasResult, error))
    {
        return false;
    }
    if (snapshot.hasResult && !ReadResult(reader, snapshot.result))
    {
        return false;
    }
    const auto checksum = reader.ReadU64();
    if (!checksum.has_value())
    {
        return false;
    }
    snapshot.eventChecksum = *checksum;
    return true;
}

[[nodiscard]] bool ReadQuantizedActor(
    ByteReader& reader,
    QuantizedActorValue& actor,
    SnapshotPayloadDecodeError& error)
{
    const auto id = reader.ReadU32();
    const auto role = reader.ReadU8();
    const auto archetype = reader.ReadU8();
    const auto positionX = reader.ReadU16();
    const auto positionZ = reader.ReadU16();
    const auto health = reader.ReadU8();
    const auto alive = reader.ReadU8();
    const auto weapon = reader.ReadU8();
    const auto cooldown = reader.ReadU16();
    const auto eliminations = reader.ReadU8();
    if (!id.has_value()
        || !role.has_value()
        || !archetype.has_value()
        || !positionX.has_value()
        || !positionZ.has_value()
        || !health.has_value()
        || !alive.has_value()
        || !weapon.has_value()
        || !cooldown.has_value()
        || !eliminations.has_value())
    {
        return false;
    }
    if (*alive > 1U)
    {
        SetError(error, SnapshotPayloadDecodeError::InvalidValue);
        return false;
    }
    actor = {
        EntityId{*id},
        static_cast<NetworkActorRole>(*role),
        static_cast<NetworkNeutralArchetype>(*archetype),
        {*positionX, *positionZ},
        *health,
        *alive == 1U,
        static_cast<NetworkWeaponType>(*weapon),
        *cooldown,
        *eliminations};
    return true;
}

[[nodiscard]] bool ReadQuantizedLoot(
    ByteReader& reader,
    QuantizedLootValue& loot,
    SnapshotPayloadDecodeError& error)
{
    const auto id = reader.ReadU32();
    const auto type = reader.ReadU8();
    const auto positionX = reader.ReadU16();
    const auto positionZ = reader.ReadU16();
    const auto active = reader.ReadU8();
    if (!id.has_value()
        || !type.has_value()
        || !positionX.has_value()
        || !positionZ.has_value()
        || !active.has_value())
    {
        return false;
    }
    if (*active > 1U)
    {
        SetError(error, SnapshotPayloadDecodeError::InvalidValue);
        return false;
    }
    loot = {
        *id,
        static_cast<NetworkLootType>(*type),
        {*positionX, *positionZ},
        *active == 1U};
    return true;
}

[[nodiscard]] bool ReadQuantizedGlobal(
    ByteReader& reader,
    QuantizedGlobalValue& global,
    SnapshotPayloadDecodeError& error)
{
    const auto phase = reader.ReadU8();
    const auto stage = reader.ReadU8();
    const auto centerX = reader.ReadU16();
    const auto centerZ = reader.ReadU16();
    const auto radius = reader.ReadU16();
    const auto aliveContenders = reader.ReadU8();
    if (!phase.has_value()
        || !stage.has_value()
        || !centerX.has_value()
        || !centerZ.has_value()
        || !radius.has_value()
        || !aliveContenders.has_value())
    {
        return false;
    }
    global.phase = static_cast<NetworkMatchPhase>(*phase);
    global.safeZoneStage = static_cast<NetworkSafeZoneStage>(*stage);
    global.safeZoneCenter = {*centerX, *centerZ};
    global.safeZoneRadius = *radius;
    global.aliveContenders = *aliveContenders;
    if (!ReadBool(reader, global.hasResult, error))
    {
        return false;
    }
    if (global.hasResult && !ReadResult(reader, global.result))
    {
        return false;
    }
    const auto checksum = reader.ReadU64();
    if (!checksum.has_value())
    {
        return false;
    }
    global.eventChecksum = *checksum;
    return true;
}

[[nodiscard]] bool ReadGlobalDelta(
    ByteReader& reader,
    QuantizedGlobalDelta& global,
    SnapshotPayloadDecodeError& error)
{
    const auto fields = reader.ReadU8();
    if (!fields.has_value())
    {
        return false;
    }
    global.fields = static_cast<GlobalField>(*fields);
    if (!IsValidGlobalFieldMask(global.fields))
    {
        SetError(error, SnapshotPayloadDecodeError::InvalidValue);
        return false;
    }
    if (HasField(global.fields, GlobalField::Phase))
    {
        const auto phase = reader.ReadU8();
        if (!phase.has_value())
        {
            return false;
        }
        global.phase = static_cast<NetworkMatchPhase>(*phase);
    }
    if (HasField(global.fields, GlobalField::SafeZone))
    {
        const auto stage = reader.ReadU8();
        const auto centerX = reader.ReadU16();
        const auto centerZ = reader.ReadU16();
        const auto radius = reader.ReadU16();
        if (!stage.has_value()
            || !centerX.has_value()
            || !centerZ.has_value()
            || !radius.has_value())
        {
            return false;
        }
        global.safeZoneStage = static_cast<NetworkSafeZoneStage>(*stage);
        global.safeZoneCenter = {*centerX, *centerZ};
        global.safeZoneRadius = *radius;
    }
    if (HasField(global.fields, GlobalField::AliveContenders))
    {
        const auto aliveContenders = reader.ReadU8();
        if (!aliveContenders.has_value())
        {
            return false;
        }
        global.aliveContenders = *aliveContenders;
    }
    if (HasField(global.fields, GlobalField::Result))
    {
        if (!ReadBool(reader, global.hasResult, error))
        {
            return false;
        }
        if (global.hasResult && !ReadResult(reader, global.result))
        {
            return false;
        }
    }
    if (HasField(global.fields, GlobalField::EventChecksum))
    {
        const auto checksum = reader.ReadU64();
        if (!checksum.has_value())
        {
            return false;
        }
        global.eventChecksum = *checksum;
    }
    return true;
}

[[nodiscard]] bool ReadActorDelta(
    ByteReader& reader,
    QuantizedActorDelta& actor,
    SnapshotPayloadDecodeError& error)
{
    const auto id = reader.ReadU32();
    const auto fields = reader.ReadU8();
    if (!id.has_value() || !fields.has_value())
    {
        return false;
    }
    actor.id = EntityId{*id};
    actor.fields = static_cast<ActorField>(*fields);
    if (actor.fields == ActorField::None
        || !IsValidActorFieldMask(actor.fields))
    {
        SetError(error, SnapshotPayloadDecodeError::InvalidValue);
        return false;
    }
    if (HasField(actor.fields, ActorField::Position))
    {
        const auto positionX = reader.ReadU16();
        const auto positionZ = reader.ReadU16();
        if (!positionX.has_value() || !positionZ.has_value())
        {
            return false;
        }
        actor.position = {*positionX, *positionZ};
    }
    if (HasField(actor.fields, ActorField::HealthAlive))
    {
        const auto health = reader.ReadU8();
        if (!health.has_value())
        {
            return false;
        }
        actor.health = *health;
        if (!ReadBool(reader, actor.alive, error))
        {
            return false;
        }
    }
    if (HasField(actor.fields, ActorField::WeaponCooldown))
    {
        const auto weapon = reader.ReadU8();
        const auto cooldown = reader.ReadU16();
        if (!weapon.has_value() || !cooldown.has_value())
        {
            return false;
        }
        actor.weapon = static_cast<NetworkWeaponType>(*weapon);
        actor.cooldownTicksRemaining = *cooldown;
    }
    if (HasField(actor.fields, ActorField::Eliminations))
    {
        const auto eliminations = reader.ReadU8();
        if (!eliminations.has_value())
        {
            return false;
        }
        actor.eliminations = *eliminations;
    }
    return true;
}

[[nodiscard]] bool ReadRemovedActor(
    ByteReader& reader,
    EntityId& actor,
    SnapshotPayloadDecodeError&)
{
    const auto id = reader.ReadU32();
    if (!id.has_value())
    {
        return false;
    }
    actor = EntityId{*id};
    return true;
}

[[nodiscard]] bool ReadLootDelta(
    ByteReader& reader,
    QuantizedLootDelta& loot,
    SnapshotPayloadDecodeError& error)
{
    const auto id = reader.ReadU32();
    const auto fields = reader.ReadU8();
    if (!id.has_value() || !fields.has_value())
    {
        return false;
    }
    loot.id = *id;
    loot.fields = static_cast<LootField>(*fields);
    if (loot.fields != LootField::Active)
    {
        SetError(error, SnapshotPayloadDecodeError::InvalidValue);
        return false;
    }
    return ReadBool(reader, loot.active, error);
}

[[nodiscard]] bool ReadRemovedLoot(
    ByteReader& reader,
    std::uint32_t& loot,
    SnapshotPayloadDecodeError&)
{
    const auto id = reader.ReadU32();
    if (!id.has_value())
    {
        return false;
    }
    loot = *id;
    return true;
}

[[nodiscard]] bool SetCanonicalDecodeError(
    const CanonicalOrder order,
    SnapshotPayloadDecodeError& error)
{
    switch (order)
    {
    case CanonicalOrder::Canonical:
        return true;
    case CanonicalOrder::Duplicate:
        SetError(error, SnapshotPayloadDecodeError::DuplicateEntity);
        return false;
    case CanonicalOrder::OutOfOrder:
        SetError(error, SnapshotPayloadDecodeError::NonCanonicalOrder);
        return false;
    }
    SetError(error, SnapshotPayloadDecodeError::InvalidValue);
    return false;
}

[[nodiscard]] bool ValidateKeyframeCanonicalOrder(
    const SnapshotPayload& payload,
    SnapshotPayloadDecodeError& error)
{
    if (payload.header.valueEncoding
        == SnapshotValueEncoding::FullPrecision)
    {
        return SetCanonicalDecodeError(
                   CheckCanonicalOrder(
                       payload.fullPrecision.actors,
                       [](const NetworkActorSnapshot& actor) {
                           return actor.id;
                       }),
                   error)
            && SetCanonicalDecodeError(
                CheckCanonicalOrder(
                    payload.fullPrecision.loot,
                    [](const NetworkLootSnapshot& loot) {
                        return loot.id;
                    }),
                error);
    }
    return SetCanonicalDecodeError(
               CheckCanonicalOrder(
                   payload.actorValues,
                   [](const QuantizedActorValue& actor) {
                       return actor.id;
                   }),
               error)
        && SetCanonicalDecodeError(
            CheckCanonicalOrder(
                payload.lootValues,
                [](const QuantizedLootValue& loot) { return loot.id; }),
            error);
}

[[nodiscard]] bool ValidateDeltaCanonicalOrder(
    const SnapshotPayload& payload,
    SnapshotPayloadDecodeError& error)
{
    const bool canonical = SetCanonicalDecodeError(
        CheckCanonicalOrder(
            payload.actorValues,
            [](const QuantizedActorValue& actor) { return actor.id; }),
        error)
        && SetCanonicalDecodeError(
            CheckCanonicalOrder(
                payload.actorDeltas,
                [](const QuantizedActorDelta& actor) { return actor.id; }),
            error)
        && SetCanonicalDecodeError(
            CheckCanonicalOrder(
                payload.removedActors,
                [](const EntityId actor) { return actor; }),
            error)
        && SetCanonicalDecodeError(
            CheckCanonicalOrder(
                payload.lootValues,
                [](const QuantizedLootValue& loot) { return loot.id; }),
            error)
        && SetCanonicalDecodeError(
            CheckCanonicalOrder(
                payload.lootDeltas,
                [](const QuantizedLootDelta& loot) { return loot.id; }),
            error)
        && SetCanonicalDecodeError(
            CheckCanonicalOrder(
                payload.removedLoot,
                [](const std::uint32_t loot) { return loot; }),
            error);
    if (!canonical)
    {
        return false;
    }
    if (HasDuplicateActorCategory(payload)
        || HasDuplicateLootCategory(payload))
    {
        SetError(error, SnapshotPayloadDecodeError::DuplicateEntity);
        return false;
    }
    return true;
}

[[nodiscard]] bool ReadQuantizedKeyframe(
    ByteReader& reader,
    SnapshotPayload& payload,
    SnapshotPayloadDecodeError& error)
{
    return ReadQuantizedGlobal(reader, payload.global, error)
        && ReadBoundedVector(
            reader,
            payload.actorValues,
            MaxSnapshotActors,
            error,
            ReadQuantizedActor)
        && ReadBoundedVector(
            reader,
            payload.lootValues,
            MaxSnapshotLoot,
            error,
            ReadQuantizedLoot);
}

[[nodiscard]] bool ReadDelta(
    ByteReader& reader,
    SnapshotPayload& payload,
    SnapshotPayloadDecodeError& error)
{
    if (!ReadGlobalDelta(reader, payload.globalDelta, error)
        || !ReadBoundedVector(
            reader,
            payload.actorValues,
            MaxSnapshotActors,
            error,
            ReadQuantizedActor)
        || !ReadBoundedVector(
            reader,
            payload.actorDeltas,
            MaxSnapshotActors,
            error,
            ReadActorDelta)
        || !ReadBoundedVector(
            reader,
            payload.removedActors,
            MaxSnapshotActors,
            error,
            ReadRemovedActor)
        || !ReadBoundedVector(
            reader,
            payload.lootValues,
            MaxSnapshotLoot,
            error,
            ReadQuantizedLoot)
        || !ReadBoundedVector(
            reader,
            payload.lootDeltas,
            MaxSnapshotLoot,
            error,
            ReadLootDelta)
        || !ReadBoundedVector(
            reader,
            payload.removedLoot,
            MaxSnapshotLoot,
            error,
            ReadRemovedLoot))
    {
        return false;
    }

    if (payload.actorValues.size()
            + payload.actorDeltas.size()
            + payload.removedActors.size()
        > MaxSnapshotActors
        || payload.lootValues.size()
                + payload.lootDeltas.size()
                + payload.removedLoot.size()
            > MaxSnapshotLoot)
    {
        SetError(error, SnapshotPayloadDecodeError::CountLimit);
        return false;
    }
    return true;
}
} // namespace

std::vector<std::byte> EncodeSnapshotPayload(const SnapshotPayload& payload)
{
    ValidatePayload(payload);

    ByteWriter writer;
    WriteHeader(writer, payload.header);
    switch (payload.header.kind)
    {
    case SnapshotPayloadKind::FullState:
    {
        const std::vector<std::byte> fullState =
            EncodeGameSnapshot(payload.fullPrecision);
        writer.WriteBytes(fullState);
        break;
    }
    case SnapshotPayloadKind::Keyframe:
        if (payload.header.valueEncoding
            == SnapshotValueEncoding::FullPrecision)
        {
            WritePrecisionKeyframe(writer, payload.fullPrecision);
        }
        else
        {
            WriteQuantizedKeyframe(writer, payload);
        }
        break;
    case SnapshotPayloadKind::Delta:
        WriteDelta(writer, payload);
        break;
    }

    std::vector<std::byte> bytes = std::move(writer).Finish();
    if (bytes.size() > MaxSnapshotPayloadBytes)
    {
        throw std::length_error{"replication snapshot exceeds payload limit"};
    }
    return bytes;
}

SnapshotPayloadDecodeResult DecodeSnapshotPayload(
    const std::span<const std::byte> bytes)
{
    if (bytes.size() > MaxSnapshotPayloadBytes)
    {
        return Failure(SnapshotPayloadDecodeError::CountLimit);
    }

    ByteReader reader{bytes};
    const auto kind = reader.ReadU8();
    const auto encoding = reader.ReadU8();
    const auto baseSnapshotId = reader.ReadU32();
    const auto payloadSnapshotId = reader.ReadU32();
    if (!kind.has_value()
        || !encoding.has_value()
        || !baseSnapshotId.has_value()
        || !payloadSnapshotId.has_value())
    {
        return Failure(ReaderFailure(reader));
    }

    SnapshotPayload payload;
    payload.header = {
        static_cast<SnapshotPayloadKind>(*kind),
        static_cast<SnapshotValueEncoding>(*encoding),
        *baseSnapshotId,
        *payloadSnapshotId};
    if (!IsValidHeader(payload.header))
    {
        return Failure(SnapshotPayloadDecodeError::InvalidValue);
    }

    SnapshotPayloadDecodeError error = SnapshotPayloadDecodeError::None;
    switch (payload.header.kind)
    {
    case SnapshotPayloadKind::FullState:
    {
        const auto body = reader.ReadBytes(reader.Remaining());
        if (!body.has_value())
        {
            return Failure(ReaderFailure(reader));
        }
        GameSnapshotDecodeResult decoded = DecodeGameSnapshot(*body);
        if (!decoded.snapshot.has_value())
        {
            return Failure(GameSnapshotFailure(decoded.error));
        }
        payload.fullPrecision = std::move(*decoded.snapshot);
        break;
    }
    case SnapshotPayloadKind::Keyframe:
        if (payload.header.valueEncoding
            == SnapshotValueEncoding::FullPrecision)
        {
            if (!ReadPrecisionKeyframe(
                    reader,
                    payload.fullPrecision,
                    error))
            {
                return Failure(
                    error == SnapshotPayloadDecodeError::None
                        ? ReaderFailure(reader)
                        : error);
            }
        }
        else if (!ReadQuantizedKeyframe(reader, payload, error))
        {
            return Failure(
                error == SnapshotPayloadDecodeError::None
                    ? ReaderFailure(reader)
                    : error);
        }
        if (!ValidateKeyframeCanonicalOrder(payload, error))
        {
            return Failure(error);
        }
        break;
    case SnapshotPayloadKind::Delta:
        if (!ReadDelta(reader, payload, error))
        {
            return Failure(
                error == SnapshotPayloadDecodeError::None
                    ? ReaderFailure(reader)
                    : error);
        }
        if (!ValidateDeltaCanonicalOrder(payload, error))
        {
            return Failure(error);
        }
        break;
    }

    if (!reader.Empty())
    {
        return Failure(SnapshotPayloadDecodeError::TrailingBytes);
    }
    try
    {
        ValidatePayload(payload);
    }
    catch (const std::length_error&)
    {
        return Failure(SnapshotPayloadDecodeError::CountLimit);
    }
    catch (const std::invalid_argument&)
    {
        return Failure(SnapshotPayloadDecodeError::InvalidValue);
    }
    return {std::move(payload), SnapshotPayloadDecodeError::None};
}
} // namespace dxa::protocol
