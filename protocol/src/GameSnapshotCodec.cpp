#include <dxa/protocol/GameSnapshotCodec.hpp>

#include <dxa/protocol/ByteCodec.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace dxa::protocol
{
namespace
{
[[nodiscard]] bool IsFinite(const NetworkVec2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.z);
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

[[nodiscard]] bool IsValidActor(const NetworkActorSnapshot& actor) noexcept
{
    if (!IsValidRole(actor.role)
        || !IsValidArchetype(actor.neutralArchetype)
        || !IsValidWeapon(actor.weapon)
        || !IsFinite(actor.position)
        || actor.health < 0
        || actor.health > 100
        || actor.alive != (actor.health > 0))
    {
        return false;
    }
    if (actor.role == NetworkActorRole::Contender)
    {
        return actor.neutralArchetype == NetworkNeutralArchetype::None;
    }
    return actor.neutralArchetype == NetworkNeutralArchetype::Melee
        || actor.neutralArchetype == NetworkNeutralArchetype::Ranged;
}

[[nodiscard]] bool IsValidLoot(const NetworkLootSnapshot& loot) noexcept
{
    return IsValidLootType(loot.type) && IsFinite(loot.position);
}

[[nodiscard]] bool Canonicalize(GameSnapshot& snapshot)
{
    if (!IsValidPhase(snapshot.phase)
        || !IsValidSafeZoneStage(snapshot.safeZoneStage)
        || !IsFinite(snapshot.safeZoneCenter)
        || !std::isfinite(snapshot.safeZoneRadius)
        || snapshot.safeZoneRadius < 0.0F
        || snapshot.actors.size() > MaxSnapshotActors
        || snapshot.loot.size() > MaxSnapshotLoot)
    {
        return false;
    }

    std::sort(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        [](const NetworkActorSnapshot& left,
           const NetworkActorSnapshot& right) {
            return left.id < right.id;
        });
    if (std::adjacent_find(
            snapshot.actors.begin(),
            snapshot.actors.end(),
            [](const NetworkActorSnapshot& left,
               const NetworkActorSnapshot& right) {
                return left.id == right.id;
            }) != snapshot.actors.end()
        || !std::all_of(
            snapshot.actors.begin(),
            snapshot.actors.end(),
            IsValidActor))
    {
        return false;
    }

    std::sort(
        snapshot.loot.begin(),
        snapshot.loot.end(),
        [](const NetworkLootSnapshot& left,
           const NetworkLootSnapshot& right) {
            return left.id < right.id;
        });
    if (std::adjacent_find(
            snapshot.loot.begin(),
            snapshot.loot.end(),
            [](const NetworkLootSnapshot& left,
               const NetworkLootSnapshot& right) {
                return left.id == right.id;
            }) != snapshot.loot.end()
        || !std::all_of(
            snapshot.loot.begin(),
            snapshot.loot.end(),
            IsValidLoot))
    {
        return false;
    }

    const std::uint32_t aliveContenders = static_cast<std::uint32_t>(
        std::count_if(
            snapshot.actors.begin(),
            snapshot.actors.end(),
            [](const NetworkActorSnapshot& actor) {
                return actor.role == NetworkActorRole::Contender
                    && actor.alive;
            }));
    if (snapshot.aliveContenders != aliveContenders)
    {
        return false;
    }

    const bool finished = snapshot.phase == NetworkMatchPhase::Finished;
    if (snapshot.hasResult != finished)
    {
        return false;
    }
    if (!snapshot.hasResult)
    {
        return true;
    }
    if (!IsValidEndReason(snapshot.result.reason)
        || snapshot.aliveContenders != 1U)
    {
        return false;
    }
    const auto winner = std::lower_bound(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        snapshot.result.winner,
        [](const NetworkActorSnapshot& actor, const EntityId id) {
            return actor.id < id;
        });
    return winner != snapshot.actors.end()
        && winner->id == snapshot.result.winner
        && winner->role == NetworkActorRole::Contender
        && winner->alive;
}

void WriteActor(ByteWriter& writer, const NetworkActorSnapshot& actor)
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

void WriteLoot(ByteWriter& writer, const NetworkLootSnapshot& loot)
{
    writer.WriteU32(loot.id);
    writer.WriteU8(static_cast<std::uint8_t>(loot.type));
    writer.WriteF32(loot.position.x);
    writer.WriteF32(loot.position.z);
    writer.WriteU8(loot.active ? 1U : 0U);
}

[[nodiscard]] GameSnapshotDecodeResult Failure(const DecodeError error)
{
    return {std::nullopt, error};
}

[[nodiscard]] GameSnapshotDecodeResult ReaderFailure(const ByteReader& reader)
{
    return Failure(
        reader.Error() == DecodeError::None
            ? DecodeError::TrailingBytes
            : reader.Error());
}
} // namespace

std::vector<std::byte> EncodeGameSnapshot(const GameSnapshot& snapshot)
{
    GameSnapshot canonical = snapshot;
    if (!Canonicalize(canonical))
    {
        throw std::invalid_argument{"game snapshot is invalid"};
    }

    ByteWriter writer;
    writer.WriteU8(static_cast<std::uint8_t>(canonical.phase));
    writer.WriteU8(static_cast<std::uint8_t>(canonical.safeZoneStage));
    writer.WriteF32(canonical.safeZoneCenter.x);
    writer.WriteF32(canonical.safeZoneCenter.z);
    writer.WriteF32(canonical.safeZoneRadius);
    writer.WriteU32(canonical.aliveContenders);
    writer.WriteU16(static_cast<std::uint16_t>(canonical.actors.size()));
    for (const NetworkActorSnapshot& actor : canonical.actors)
    {
        WriteActor(writer, actor);
    }
    writer.WriteU16(static_cast<std::uint16_t>(canonical.loot.size()));
    for (const NetworkLootSnapshot& loot : canonical.loot)
    {
        WriteLoot(writer, loot);
    }
    writer.WriteU8(canonical.hasResult ? 1U : 0U);
    if (canonical.hasResult)
    {
        writer.WriteU32(canonical.result.winner.value);
        writer.WriteU8(static_cast<std::uint8_t>(canonical.result.reason));
        writer.WriteU32(canonical.result.finishedTick);
    }
    writer.WriteU64(canonical.eventChecksum);

    std::vector<std::byte> bytes = std::move(writer).Finish();
    if (bytes.size() > MaxSnapshotPayloadBytes)
    {
        throw std::length_error{"game snapshot exceeds fragment payload limit"};
    }
    return bytes;
}

GameSnapshotDecodeResult DecodeGameSnapshot(
    const std::span<const std::byte> bytes)
{
    if (bytes.size() > MaxSnapshotPayloadBytes)
    {
        return Failure(DecodeError::CountLimit);
    }

    ByteReader reader{bytes};
    const auto phaseValue = reader.ReadU8();
    const auto stageValue = reader.ReadU8();
    const auto centerX = reader.ReadF32();
    const auto centerZ = reader.ReadF32();
    const auto radius = reader.ReadF32();
    const auto aliveContenders = reader.ReadU32();
    const auto actorCount = reader.ReadU16();
    if (!phaseValue.has_value()
        || !stageValue.has_value()
        || !centerX.has_value()
        || !centerZ.has_value()
        || !radius.has_value()
        || !aliveContenders.has_value()
        || !actorCount.has_value())
    {
        return ReaderFailure(reader);
    }
    if (*actorCount > MaxSnapshotActors)
    {
        return Failure(DecodeError::CountLimit);
    }

    GameSnapshot snapshot;
    snapshot.phase = static_cast<NetworkMatchPhase>(*phaseValue);
    snapshot.safeZoneStage = static_cast<NetworkSafeZoneStage>(*stageValue);
    snapshot.safeZoneCenter = {*centerX, *centerZ};
    snapshot.safeZoneRadius = *radius;
    snapshot.aliveContenders = *aliveContenders;
    snapshot.actors.reserve(*actorCount);
    for (std::uint16_t index = 0U; index < *actorCount; ++index)
    {
        const auto id = reader.ReadU32();
        const auto role = reader.ReadU8();
        const auto archetype = reader.ReadU8();
        const auto positionX = reader.ReadF32();
        const auto positionZ = reader.ReadF32();
        const auto healthBits = reader.ReadU32();
        const auto alive = reader.ReadU8();
        const auto weapon = reader.ReadU8();
        const auto cooldown = reader.ReadU32();
        const auto eliminations = reader.ReadU32();
        if (!id.has_value()
            || !role.has_value()
            || !archetype.has_value()
            || !positionX.has_value()
            || !positionZ.has_value()
            || !healthBits.has_value()
            || !alive.has_value()
            || !weapon.has_value()
            || !cooldown.has_value()
            || !eliminations.has_value())
        {
            return ReaderFailure(reader);
        }
        if (*alive > 1U)
        {
            return Failure(DecodeError::InvalidValue);
        }
        snapshot.actors.push_back(NetworkActorSnapshot{
            EntityId{*id},
            static_cast<NetworkActorRole>(*role),
            static_cast<NetworkNeutralArchetype>(*archetype),
            {*positionX, *positionZ},
            std::bit_cast<std::int32_t>(*healthBits),
            *alive == 1U,
            static_cast<NetworkWeaponType>(*weapon),
            *cooldown,
            *eliminations});
    }

    const auto lootCount = reader.ReadU16();
    if (!lootCount.has_value())
    {
        return ReaderFailure(reader);
    }
    if (*lootCount > MaxSnapshotLoot)
    {
        return Failure(DecodeError::CountLimit);
    }
    snapshot.loot.reserve(*lootCount);
    for (std::uint16_t index = 0U; index < *lootCount; ++index)
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
            return ReaderFailure(reader);
        }
        if (*active > 1U)
        {
            return Failure(DecodeError::InvalidValue);
        }
        snapshot.loot.push_back(NetworkLootSnapshot{
            *id,
            static_cast<NetworkLootType>(*type),
            {*positionX, *positionZ},
            *active == 1U});
    }

    const auto hasResult = reader.ReadU8();
    if (!hasResult.has_value())
    {
        return ReaderFailure(reader);
    }
    if (*hasResult > 1U)
    {
        return Failure(DecodeError::InvalidValue);
    }
    snapshot.hasResult = *hasResult == 1U;
    if (snapshot.hasResult)
    {
        const auto winner = reader.ReadU32();
        const auto reason = reader.ReadU8();
        const auto finishedTick = reader.ReadU32();
        if (!winner.has_value()
            || !reason.has_value()
            || !finishedTick.has_value())
        {
            return ReaderFailure(reader);
        }
        snapshot.result = {
            EntityId{*winner},
            static_cast<NetworkMatchEndReason>(*reason),
            *finishedTick};
    }
    const auto eventChecksum = reader.ReadU64();
    if (!eventChecksum.has_value())
    {
        return ReaderFailure(reader);
    }
    snapshot.eventChecksum = *eventChecksum;

    if (!Canonicalize(snapshot))
    {
        return Failure(DecodeError::InvalidValue);
    }
    if (!reader.Empty())
    {
        return ReaderFailure(reader);
    }
    return {std::move(snapshot), DecodeError::None};
}
} // namespace dxa::protocol
