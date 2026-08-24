#pragma once

#include <compare>
#include <cstdint>

namespace dxa::protocol
{
struct PlayerId
{
    std::uint32_t value = 0;

    [[nodiscard]] auto operator<=>(const PlayerId&) const = default;
};

struct RoomId
{
    std::uint32_t value = 0;

    [[nodiscard]] auto operator<=>(const RoomId&) const = default;
};

struct MatchId
{
    std::uint64_t value = 0;

    [[nodiscard]] auto operator<=>(const MatchId&) const = default;
};

struct EntityId
{
    std::uint32_t value = 0;

    [[nodiscard]] auto operator<=>(const EntityId&) const = default;
};
} // namespace dxa::protocol
