#include <dxa/protocol/TcpFrame.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace
{
[[nodiscard]] std::array<std::byte, dxa::protocol::TcpFrameHeaderBytes> Header(
    const std::uint32_t payloadBytes,
    const std::uint16_t version = dxa::protocol::ProtocolVersion,
    const std::uint16_t type = static_cast<std::uint16_t>(
        dxa::protocol::MessageType::ClientHello))
{
    return {
        std::byte{0x44}, std::byte{0x58}, std::byte{0x41}, std::byte{0x31},
        static_cast<std::byte>(version & 0xFFU),
        static_cast<std::byte>((version >> 8U) & 0xFFU),
        static_cast<std::byte>(type & 0xFFU),
        static_cast<std::byte>((type >> 8U) & 0xFFU),
        static_cast<std::byte>(payloadBytes & 0xFFU),
        static_cast<std::byte>((payloadBytes >> 8U) & 0xFFU),
        static_cast<std::byte>((payloadBytes >> 16U) & 0xFFU),
        static_cast<std::byte>((payloadBytes >> 24U) & 0xFFU)};
}
} // namespace

TEST(TcpFrame, EncodesAndDecodesTwelveByteHeaderAndPayload)
{
    const dxa::protocol::EncodedMessage message{
        dxa::protocol::MessageType::ClientHello,
        {std::byte{0x11}, std::byte{0x22}}};

    const std::vector<std::byte> frame = dxa::protocol::EncodeTcpFrame(message);

    const std::vector<std::byte> expected{
        std::byte{0x44}, std::byte{0x58}, std::byte{0x41}, std::byte{0x31},
        std::byte{0x01}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x11}, std::byte{0x22}};
    EXPECT_EQ(expected, frame);

    const auto decoded = dxa::protocol::DecodeTcpFrameHeader(
        std::span<const std::byte>{frame}.first(dxa::protocol::TcpFrameHeaderBytes));
    ASSERT_TRUE(decoded.header.has_value());
    EXPECT_EQ(dxa::protocol::FrameHeaderError::None, decoded.error);
    EXPECT_EQ(dxa::protocol::ProtocolVersion, decoded.header->version);
    EXPECT_EQ(dxa::protocol::MessageType::ClientHello, decoded.header->type);
    EXPECT_EQ(2U, decoded.header->payloadBytes);
}

TEST(TcpFrame, AcceptsExactMaximumAndRejectsOneByteAbove)
{
    dxa::protocol::EncodedMessage maximum{
        dxa::protocol::MessageType::RoomListResponse,
        std::vector<std::byte>(dxa::protocol::MaxTcpPayloadBytes)};
    EXPECT_EQ(
        dxa::protocol::MaxTcpFrameBytes,
        dxa::protocol::EncodeTcpFrame(maximum).size());

    maximum.payload.push_back(std::byte{0x00});
    EXPECT_THROW(
        (void)dxa::protocol::EncodeTcpFrame(maximum),
        std::length_error);

    const auto oversized = Header(
        static_cast<std::uint32_t>(dxa::protocol::MaxTcpPayloadBytes + 1U));
    const auto decoded = dxa::protocol::DecodeTcpFrameHeader(oversized);
    EXPECT_FALSE(decoded.header.has_value());
    EXPECT_EQ(dxa::protocol::FrameHeaderError::FrameTooLarge, decoded.error);
}

TEST(TcpFrame, RejectsWrongMagicVersionAndMessageType)
{
    auto badMagic = Header(0U);
    badMagic[0] = std::byte{0x00};
    EXPECT_EQ(
        dxa::protocol::FrameHeaderError::BadMagic,
        dxa::protocol::DecodeTcpFrameHeader(badMagic).error);

    const auto badVersion = Header(
        0U,
        static_cast<std::uint16_t>(dxa::protocol::ProtocolVersion + 1U));
    EXPECT_EQ(
        dxa::protocol::FrameHeaderError::UnsupportedVersion,
        dxa::protocol::DecodeTcpFrameHeader(badVersion).error);

    const auto unknownType = Header(0U, dxa::protocol::ProtocolVersion, 99U);
    EXPECT_EQ(
        dxa::protocol::FrameHeaderError::UnknownMessageType,
        dxa::protocol::DecodeTcpFrameHeader(unknownType).error);
}

TEST(TcpFrame, RejectsHeaderWithWrongByteCount)
{
    const std::array shortHeader{std::byte{0x44}, std::byte{0x58}};
    const auto decoded = dxa::protocol::DecodeTcpFrameHeader(shortHeader);
    EXPECT_FALSE(decoded.header.has_value());
    EXPECT_EQ(dxa::protocol::FrameHeaderError::InvalidHeaderSize, decoded.error);
}
