#include <dxa/protocol/ByteCodec.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

TEST(ByteCodec, WritesAndReadsLittleEndianValues)
{
    dxa::protocol::ByteWriter writer;
    writer.WriteU8(0xA5U);
    writer.WriteU16(0x1234U);
    writer.WriteU32(0x89ABCDEFU);
    writer.WriteU64(0x0102030405060708ULL);
    const std::vector<std::byte> bytes = std::move(writer).Finish();

    const std::vector<std::byte> expected{
        std::byte{0xA5},
        std::byte{0x34}, std::byte{0x12},
        std::byte{0xEF}, std::byte{0xCD}, std::byte{0xAB}, std::byte{0x89},
        std::byte{0x08}, std::byte{0x07}, std::byte{0x06}, std::byte{0x05},
        std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01}};
    EXPECT_EQ(expected, bytes);

    dxa::protocol::ByteReader reader{bytes};
    const auto u8 = reader.ReadU8();
    const auto u16 = reader.ReadU16();
    const auto u32 = reader.ReadU32();
    const auto u64 = reader.ReadU64();
    ASSERT_TRUE(u8.has_value());
    ASSERT_TRUE(u16.has_value());
    ASSERT_TRUE(u32.has_value());
    ASSERT_TRUE(u64.has_value());
    EXPECT_EQ(0xA5U, *u8);
    EXPECT_EQ(0x1234U, *u16);
    EXPECT_EQ(0x89ABCDEFU, *u32);
    EXPECT_EQ(0x0102030405060708ULL, *u64);
    EXPECT_TRUE(reader.Empty());
    EXPECT_EQ(dxa::protocol::DecodeError::None, reader.Error());
}

TEST(ByteCodec, RoundTripsByteArrayAndMaximumString)
{
    const std::array source{
        std::byte{0x00},
        std::byte{0x7F},
        std::byte{0xFF}};
    const std::string text(255U, 'x');
    dxa::protocol::ByteWriter writer;
    writer.WriteBytes(source);
    writer.WriteString8(text);
    const std::vector<std::byte> encoded = std::move(writer).Finish();

    dxa::protocol::ByteReader reader{encoded};
    const auto bytes = reader.ReadBytes(source.size());
    const auto decodedText = reader.ReadString8(255U);

    ASSERT_TRUE(bytes.has_value());
    ASSERT_TRUE(decodedText.has_value());
    EXPECT_EQ(std::vector<std::byte>(source.begin(), source.end()), *bytes);
    EXPECT_EQ(text, *decodedText);
    EXPECT_TRUE(reader.Empty());
}

TEST(ByteCodec, RejectsTruncatedReadAndLatchesFirstError)
{
    constexpr std::array oneByte{std::byte{0x01}};
    dxa::protocol::ByteReader reader{oneByte};

    EXPECT_FALSE(reader.ReadU32().has_value());
    EXPECT_EQ(dxa::protocol::DecodeError::Truncated, reader.Error());
    EXPECT_FALSE(reader.ReadU8().has_value());
    EXPECT_EQ(1U, reader.Remaining());
}

TEST(ByteCodec, RejectsStringOutsideWriterAndReaderLimits)
{
    dxa::protocol::ByteWriter writer;
    EXPECT_THROW(writer.WriteString8(std::string(256U, 'x')), std::length_error);

    dxa::protocol::ByteWriter encoded;
    encoded.WriteString8("abcd");
    const std::vector<std::byte> encodedBytes = std::move(encoded).Finish();
    dxa::protocol::ByteReader reader{encodedBytes};
    EXPECT_FALSE(reader.ReadString8(3U).has_value());
    EXPECT_EQ(dxa::protocol::DecodeError::CountLimit, reader.Error());
}
