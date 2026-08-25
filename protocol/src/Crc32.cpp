#include <dxa/protocol/Crc32.hpp>

namespace dxa::protocol
{
std::uint32_t Crc32(const std::span<const std::byte> bytes) noexcept
{
    constexpr std::uint32_t Polynomial = 0xEDB88320U;
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const std::byte byte : bytes)
    {
        crc ^= std::to_integer<std::uint8_t>(byte);
        for (std::uint32_t bit = 0U; bit < 8U; ++bit)
        {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (Polynomial & mask);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}
} // namespace dxa::protocol
