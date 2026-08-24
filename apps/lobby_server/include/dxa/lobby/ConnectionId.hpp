#pragma once

#include <compare>
#include <cstdint>

namespace dxa::lobby
{
struct ConnectionId
{
    std::uint64_t value = 0;

    [[nodiscard]] auto operator<=>(const ConnectionId&) const = default;
};
} // namespace dxa::lobby
