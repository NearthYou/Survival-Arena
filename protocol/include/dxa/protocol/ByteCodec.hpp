#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dxa::protocol
{
enum class DecodeError
{
    None,
    Truncated,
    InvalidValue,
    CountLimit,
    TrailingBytes
};

class ByteWriter
{
public:
    void WriteU8(std::uint8_t value);
    void WriteU16(std::uint16_t value);
    void WriteU32(std::uint32_t value);
    void WriteU64(std::uint64_t value);
    void WriteBytes(std::span<const std::byte> bytes);
    void WriteString8(std::string_view value);

    [[nodiscard]] std::vector<std::byte> Finish() && noexcept;

private:
    std::vector<std::byte> bytes_;
};

class ByteReader
{
public:
    explicit ByteReader(std::span<const std::byte> bytes) noexcept;

    [[nodiscard]] std::optional<std::uint8_t> ReadU8() noexcept;
    [[nodiscard]] std::optional<std::uint16_t> ReadU16() noexcept;
    [[nodiscard]] std::optional<std::uint32_t> ReadU32() noexcept;
    [[nodiscard]] std::optional<std::uint64_t> ReadU64() noexcept;
    [[nodiscard]] std::optional<std::vector<std::byte>> ReadBytes(std::size_t count);
    [[nodiscard]] std::optional<std::string> ReadString8(std::size_t maximum);

    [[nodiscard]] bool Empty() const noexcept;
    [[nodiscard]] std::size_t Remaining() const noexcept;
    [[nodiscard]] DecodeError Error() const noexcept;

private:
    [[nodiscard]] bool CanRead(std::size_t count) noexcept;
    void SetError(DecodeError error) noexcept;

    std::span<const std::byte> bytes_;
    std::size_t offset_ = 0;
    DecodeError error_ = DecodeError::None;
};
} // namespace dxa::protocol
