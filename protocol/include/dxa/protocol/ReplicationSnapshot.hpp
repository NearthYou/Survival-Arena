#pragma once

#include <dxa/protocol/GameSnapshot.hpp>
#include <dxa/protocol/GameTypes.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace dxa::protocol
{
enum class SnapshotPayloadKind : std::uint8_t
{
    FullState = 1,
    Keyframe = 2,
    Delta = 3
};

enum class SnapshotValueEncoding : std::uint8_t
{
    FullPrecision = 1,
    Quantized = 2
};

enum class ActorField : std::uint8_t
{
    None = 0U,
    Position = 1U << 0U,
    HealthAlive = 1U << 1U,
    WeaponCooldown = 1U << 2U,
    Eliminations = 1U << 3U
};

enum class LootField : std::uint8_t
{
    None = 0U,
    Active = 1U << 0U
};

enum class GlobalField : std::uint8_t
{
    None = 0U,
    Phase = 1U << 0U,
    SafeZone = 1U << 1U,
    AliveContenders = 1U << 2U,
    Result = 1U << 3U,
    EventChecksum = 1U << 4U
};

namespace detail
{
template <typename Enum>
[[nodiscard]] constexpr auto ToUnderlying(const Enum value) noexcept
{
    return static_cast<std::underlying_type_t<Enum>>(value);
}

template <typename Enum>
[[nodiscard]] constexpr bool HasOnlyKnownBits(
    const Enum value,
    const Enum knownBits) noexcept
{
    using Underlying = std::underlying_type_t<Enum>;
    const Underlying unknownBits = static_cast<Underlying>(
        ToUnderlying(value)
        & static_cast<Underlying>(~ToUnderlying(knownBits)));
    return unknownBits == 0U;
}

inline void ValidateQuantizationBounds(
    const float minimum,
    const float maximum)
{
    if (!std::isfinite(minimum)
        || !std::isfinite(maximum)
        || minimum >= maximum)
    {
        throw std::invalid_argument{"quantization bounds are invalid"};
    }
}
} // namespace detail

[[nodiscard]] constexpr ActorField operator|(
    const ActorField left,
    const ActorField right) noexcept
{
    return static_cast<ActorField>(
        detail::ToUnderlying(left) | detail::ToUnderlying(right));
}

[[nodiscard]] constexpr LootField operator|(
    const LootField left,
    const LootField right) noexcept
{
    return static_cast<LootField>(
        detail::ToUnderlying(left) | detail::ToUnderlying(right));
}

[[nodiscard]] constexpr GlobalField operator|(
    const GlobalField left,
    const GlobalField right) noexcept
{
    return static_cast<GlobalField>(
        detail::ToUnderlying(left) | detail::ToUnderlying(right));
}

inline constexpr ActorField AllActorFields =
    ActorField::Position
    | ActorField::HealthAlive
    | ActorField::WeaponCooldown
    | ActorField::Eliminations;
inline constexpr LootField AllLootFields = LootField::Active;
inline constexpr GlobalField AllGlobalFields =
    GlobalField::Phase
    | GlobalField::SafeZone
    | GlobalField::AliveContenders
    | GlobalField::Result
    | GlobalField::EventChecksum;

[[nodiscard]] constexpr bool IsValidActorFieldMask(
    const ActorField fields) noexcept
{
    return detail::HasOnlyKnownBits(fields, AllActorFields);
}

[[nodiscard]] constexpr bool IsValidLootFieldMask(
    const LootField fields) noexcept
{
    return detail::HasOnlyKnownBits(fields, AllLootFields);
}

[[nodiscard]] constexpr bool IsValidGlobalFieldMask(
    const GlobalField fields) noexcept
{
    return detail::HasOnlyKnownBits(fields, AllGlobalFields);
}

struct QuantizedVec2
{
    std::uint16_t x = 0U;
    std::uint16_t z = 0U;

    [[nodiscard]] bool operator==(const QuantizedVec2&) const = default;
};

struct QuantizedActorValue
{
    EntityId id;
    NetworkActorRole role = NetworkActorRole::Contender;
    NetworkNeutralArchetype neutralArchetype =
        NetworkNeutralArchetype::None;
    QuantizedVec2 position;
    std::uint8_t health = 100U;
    bool alive = true;
    NetworkWeaponType weapon = NetworkWeaponType::Blade;
    std::uint16_t cooldownTicksRemaining = 0U;
    std::uint8_t eliminations = 0U;

    [[nodiscard]] bool operator==(
        const QuantizedActorValue&) const = default;
};

struct QuantizedLootValue
{
    std::uint32_t id = 0U;
    NetworkLootType type = NetworkLootType::Rifle;
    QuantizedVec2 position;
    bool active = true;

    [[nodiscard]] bool operator==(
        const QuantizedLootValue&) const = default;
};

struct QuantizedGlobalValue
{
    NetworkMatchPhase phase = NetworkMatchPhase::Waiting;
    NetworkSafeZoneStage safeZoneStage = NetworkSafeZoneStage::Stage1;
    QuantizedVec2 safeZoneCenter;
    std::uint16_t safeZoneRadius = 0U;
    std::uint8_t aliveContenders = 0U;
    NetworkMatchResult result;
    bool hasResult = false;
    std::uint64_t eventChecksum = 0U;

    [[nodiscard]] bool operator==(
        const QuantizedGlobalValue&) const = default;
};

struct QuantizedActorDelta
{
    EntityId id;
    ActorField fields = ActorField::None;
    QuantizedVec2 position;
    std::uint8_t health = 100U;
    bool alive = true;
    NetworkWeaponType weapon = NetworkWeaponType::Blade;
    std::uint16_t cooldownTicksRemaining = 0U;
    std::uint8_t eliminations = 0U;

    [[nodiscard]] bool operator==(
        const QuantizedActorDelta&) const = default;
};

struct QuantizedLootDelta
{
    std::uint32_t id = 0U;
    LootField fields = LootField::None;
    bool active = true;

    [[nodiscard]] bool operator==(
        const QuantizedLootDelta&) const = default;
};

struct QuantizedGlobalDelta
{
    GlobalField fields = GlobalField::None;
    NetworkMatchPhase phase = NetworkMatchPhase::Waiting;
    NetworkSafeZoneStage safeZoneStage = NetworkSafeZoneStage::Stage1;
    QuantizedVec2 safeZoneCenter;
    std::uint16_t safeZoneRadius = 0U;
    std::uint8_t aliveContenders = 0U;
    NetworkMatchResult result;
    bool hasResult = false;
    std::uint64_t eventChecksum = 0U;

    [[nodiscard]] bool operator==(
        const QuantizedGlobalDelta&) const = default;
};

struct SnapshotPayloadHeader
{
    SnapshotPayloadKind kind = SnapshotPayloadKind::FullState;
    SnapshotValueEncoding valueEncoding =
        SnapshotValueEncoding::FullPrecision;
    std::uint32_t baseSnapshotId = 0U;
    std::uint32_t payloadSnapshotId = 0U;

    [[nodiscard]] bool operator==(
        const SnapshotPayloadHeader&) const = default;
};

struct SnapshotPayload
{
    SnapshotPayloadHeader header;
    GameSnapshot fullPrecision;
    QuantizedGlobalValue global;
    std::vector<QuantizedActorValue> actorValues;
    std::vector<QuantizedLootValue> lootValues;
    std::vector<QuantizedActorDelta> actorDeltas;
    std::vector<QuantizedLootDelta> lootDeltas;
    std::vector<EntityId> removedActors;
    std::vector<std::uint32_t> removedLoot;
    QuantizedGlobalDelta globalDelta;

    [[nodiscard]] bool operator==(const SnapshotPayload&) const = default;
};

[[nodiscard]] inline std::uint16_t QuantizeCoordinate(
    const float value,
    const float minimum,
    const float maximum)
{
    detail::ValidateQuantizationBounds(minimum, maximum);
    if (!std::isfinite(value))
    {
        throw std::invalid_argument{"coordinate is not finite"};
    }
    if (value < minimum || value > maximum)
    {
        throw std::out_of_range{"coordinate is outside quantization bounds"};
    }

    constexpr double encodedMaximum =
        static_cast<double>(std::numeric_limits<std::uint16_t>::max());
    const double ratio =
        (static_cast<double>(value) - static_cast<double>(minimum))
        / (static_cast<double>(maximum) - static_cast<double>(minimum));
    return static_cast<std::uint16_t>(std::llround(ratio * encodedMaximum));
}

[[nodiscard]] inline float DequantizeCoordinate(
    const std::uint16_t value,
    const float minimum,
    const float maximum)
{
    detail::ValidateQuantizationBounds(minimum, maximum);
    constexpr double encodedMaximum =
        static_cast<double>(std::numeric_limits<std::uint16_t>::max());
    const double ratio = static_cast<double>(value) / encodedMaximum;
    return static_cast<float>(
        static_cast<double>(minimum)
        + ratio
            * (static_cast<double>(maximum) - static_cast<double>(minimum)));
}

[[nodiscard]] inline std::uint16_t QuantizeSafeZoneRadius(
    const float radius,
    const float maximum)
{
    return QuantizeCoordinate(radius, 0.0F, maximum);
}

[[nodiscard]] inline float DequantizeSafeZoneRadius(
    const std::uint16_t radius,
    const float maximum)
{
    return DequantizeCoordinate(radius, 0.0F, maximum);
}

[[nodiscard]] inline std::uint8_t QuantizeHealth(const std::int32_t health)
{
    if (health < 0 || health > 100)
    {
        throw std::out_of_range{"health exceeds uint8 gameplay bounds"};
    }
    return static_cast<std::uint8_t>(health);
}

[[nodiscard]] inline std::uint16_t QuantizeCooldownTicks(
    const std::uint32_t cooldownTicks)
{
    if (cooldownTicks > std::numeric_limits<std::uint16_t>::max())
    {
        throw std::out_of_range{"cooldown exceeds uint16 bounds"};
    }
    return static_cast<std::uint16_t>(cooldownTicks);
}

[[nodiscard]] inline std::uint8_t QuantizeEliminations(
    const std::uint32_t eliminations)
{
    if (eliminations > std::numeric_limits<std::uint8_t>::max())
    {
        throw std::out_of_range{"eliminations exceed uint8 bounds"};
    }
    return static_cast<std::uint8_t>(eliminations);
}

[[nodiscard]] inline std::uint8_t QuantizeAliveContenders(
    const std::uint32_t aliveContenders)
{
    if (aliveContenders > RoomCapacity)
    {
        throw std::out_of_range{"alive contenders exceed room capacity"};
    }
    return static_cast<std::uint8_t>(aliveContenders);
}
} // namespace dxa::protocol
