#include <dxa/protocol/ByteCodec.hpp>

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>
#include <utility>

namespace dxa::protocol
{
namespace
{
[[nodiscard]] std::uint8_t ByteValue(const std::byte value) noexcept
{
    return std::to_integer<std::uint8_t>(value);
}
} // namespace

void ByteWriter::WriteU8(const std::uint8_t value)
{
    bytes_.push_back(static_cast<std::byte>(value));
}

void ByteWriter::WriteU16(std::uint16_t value)
{
    for (std::uint32_t byte = 0; byte < 2U; ++byte)
    {
        bytes_.push_back(static_cast<std::byte>(value & 0xFFU));
        value = static_cast<std::uint16_t>(value >> 8U);
    }
}

void ByteWriter::WriteU32(std::uint32_t value)
{
    for (std::uint32_t byte = 0; byte < 4U; ++byte)
    {
        bytes_.push_back(static_cast<std::byte>(value & 0xFFU));
        value >>= 8U;
    }
}

void ByteWriter::WriteU64(std::uint64_t value)
{
    for (std::uint32_t byte = 0; byte < 8U; ++byte)
    {
        bytes_.push_back(static_cast<std::byte>(value & 0xFFULL));
        value >>= 8U;
    }
}

void ByteWriter::WriteF32(const float value)
{
    WriteU32(std::bit_cast<std::uint32_t>(value));
}

void ByteWriter::WriteBytes(const std::span<const std::byte> bytes)
{
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
}

void ByteWriter::WriteString8(const std::string_view value)
{
    if (value.size() > std::numeric_limits<std::uint8_t>::max())
    {
        throw std::length_error{"byte codec string exceeds uint8 length"};
    }

    WriteU8(static_cast<std::uint8_t>(value.size()));
    for (const char character : value)
    {
        bytes_.push_back(static_cast<std::byte>(
            static_cast<unsigned char>(character)));
    }
}

std::vector<std::byte> ByteWriter::Finish() && noexcept
{
    return std::move(bytes_);
}

ByteReader::ByteReader(const std::span<const std::byte> bytes) noexcept
    : bytes_{bytes}
{
}

std::optional<std::uint8_t> ByteReader::ReadU8() noexcept
{
    if (!CanRead(1U))
    {
        return std::nullopt;
    }
    return ByteValue(bytes_[offset_++]);
}

std::optional<std::uint16_t> ByteReader::ReadU16() noexcept
{
    if (!CanRead(2U))
    {
        return std::nullopt;
    }
    const std::uint16_t result = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(ByteValue(bytes_[offset_]))
        | static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(ByteValue(bytes_[offset_ + 1U])) << 8U));
    offset_ += 2U;
    return result;
}

std::optional<std::uint32_t> ByteReader::ReadU32() noexcept
{
    if (!CanRead(4U))
    {
        return std::nullopt;
    }
    std::uint32_t result = 0;
    for (std::uint32_t byte = 0; byte < 4U; ++byte)
    {
        result |= static_cast<std::uint32_t>(ByteValue(bytes_[offset_ + byte]))
            << (byte * 8U);
    }
    offset_ += 4U;
    return result;
}

std::optional<std::uint64_t> ByteReader::ReadU64() noexcept
{
    if (!CanRead(8U))
    {
        return std::nullopt;
    }
    std::uint64_t result = 0;
    for (std::uint32_t byte = 0; byte < 8U; ++byte)
    {
        result |= static_cast<std::uint64_t>(ByteValue(bytes_[offset_ + byte]))
            << (byte * 8U);
    }
    offset_ += 8U;
    return result;
}

std::optional<float> ByteReader::ReadF32() noexcept
{
    const std::optional<std::uint32_t> bits = ReadU32();
    return bits.has_value()
        ? std::optional<float>{std::bit_cast<float>(*bits)}
        : std::nullopt;
}

std::optional<std::vector<std::byte>> ByteReader::ReadBytes(const std::size_t count)
{
    if (!CanRead(count))
    {
        return std::nullopt;
    }
    std::vector<std::byte> result{
        bytes_.begin() + static_cast<std::ptrdiff_t>(offset_),
        bytes_.begin() + static_cast<std::ptrdiff_t>(offset_ + count)};
    offset_ += count;
    return result;
}

std::optional<std::string> ByteReader::ReadString8(const std::size_t maximum)
{
    const std::optional<std::uint8_t> encodedCount = ReadU8();
    if (!encodedCount.has_value())
    {
        return std::nullopt;
    }
    const std::size_t count = *encodedCount;
    if (count > maximum)
    {
        SetError(DecodeError::CountLimit);
        return std::nullopt;
    }
    if (!CanRead(count))
    {
        return std::nullopt;
    }

    std::string result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        result.push_back(static_cast<char>(ByteValue(bytes_[offset_ + index])));
    }
    offset_ += count;
    return result;
}

bool ByteReader::Empty() const noexcept
{
    return offset_ == bytes_.size();
}

std::size_t ByteReader::Remaining() const noexcept
{
    return bytes_.size() - offset_;
}

DecodeError ByteReader::Error() const noexcept
{
    return error_;
}

bool ByteReader::CanRead(const std::size_t count) noexcept
{
    if (error_ != DecodeError::None)
    {
        return false;
    }
    if (count > Remaining())
    {
        SetError(DecodeError::Truncated);
        return false;
    }
    return true;
}

void ByteReader::SetError(const DecodeError error) noexcept
{
    if (error_ == DecodeError::None)
    {
        error_ = error;
    }
}
} // namespace dxa::protocol
