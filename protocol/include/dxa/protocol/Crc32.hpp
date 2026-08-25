#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace dxa::protocol
{
[[nodiscard]] std::uint32_t Crc32(
    std::span<const std::byte> bytes) noexcept;
} // namespace dxa::protocol
