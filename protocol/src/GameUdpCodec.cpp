#include <dxa/protocol/GameUdpCodec.hpp>

#include <dxa/protocol/Crc32.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace dxa::protocol
{
namespace
{
constexpr std::array UdpMagic{
    std::byte{0x44},
    std::byte{0x58},
    std::byte{0x55},
    std::byte{0x31}};
constexpr std::uint8_t MoveDestinationFlag = 1U << 0U;
constexpr std::uint8_t AttackTargetFlag = 1U << 1U;
constexpr std::uint8_t KnownInputFlags =
    MoveDestinationFlag | AttackTargetFlag;

template <typename... Functions>
struct Overloaded : Functions...
{
    using Functions::operator()...;
};

template <typename... Functions>
Overloaded(Functions...) -> Overloaded<Functions...>;

[[nodiscard]] bool IsKnownDatagramType(const UdpDatagramType type) noexcept
{
    switch (type)
    {
    case UdpDatagramType::Bind:
    case UdpDatagramType::BindAccepted:
    case UdpDatagramType::ClientInput:
    case UdpDatagramType::SnapshotFragment:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsFinite(const NetworkVec2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.z);
}

[[nodiscard]] bool IsValidInput(const ClientInput& value) noexcept
{
    return value.inputSequence != 0U
        && (!value.hasMoveDestination || IsFinite(value.moveDestination));
}

[[nodiscard]] std::size_t ExpectedFragmentCount(
    const std::size_t fullPayloadBytes) noexcept
{
    return fullPayloadBytes == 0U
        ? 0U
        : (fullPayloadBytes + MaxSnapshotFragmentPayloadBytes - 1U)
            / MaxSnapshotFragmentPayloadBytes;
}

[[nodiscard]] bool IsValidFragment(const SnapshotFragment& value) noexcept
{
    if (value.snapshotId == 0U
        || value.fullPayloadBytes == 0U
        || value.fullPayloadBytes > MaxSnapshotPayloadBytes
        || value.fragmentCount == 0U
        || value.fragmentCount > MaxSnapshotFragments
        || value.fragmentIndex >= value.fragmentCount)
    {
        return false;
    }

    const std::size_t expectedCount = ExpectedFragmentCount(
        value.fullPayloadBytes);
    if (value.fragmentCount != expectedCount)
    {
        return false;
    }
    const std::size_t offset = static_cast<std::size_t>(value.fragmentIndex)
        * MaxSnapshotFragmentPayloadBytes;
    if (offset >= value.fullPayloadBytes)
    {
        return false;
    }
    const std::size_t expectedBytes = std::min(
        MaxSnapshotFragmentPayloadBytes,
        static_cast<std::size_t>(value.fullPayloadBytes) - offset);
    return value.bytes.size() == expectedBytes;
}

template <typename Writer>
[[nodiscard]] std::vector<std::byte> EncodePayload(Writer write)
{
    ByteWriter writer;
    write(writer);
    return std::move(writer).Finish();
}

[[nodiscard]] EncodedDatagram EncodeFrame(
    const UdpDatagramType type,
    const std::span<const std::byte> payload)
{
    if (!IsKnownDatagramType(type))
    {
        throw std::invalid_argument{"UDP datagram type is invalid"};
    }
    if (payload.size() > MaxUdpDatagramBytes - UdpHeaderBytes
        || payload.size() > std::numeric_limits<std::uint16_t>::max())
    {
        throw std::length_error{"UDP datagram exceeds 1200-byte limit"};
    }

    ByteWriter writer;
    writer.WriteBytes(UdpMagic);
    writer.WriteU16(ProtocolVersion);
    writer.WriteU8(static_cast<std::uint8_t>(type));
    writer.WriteU8(0U);
    writer.WriteU16(static_cast<std::uint16_t>(payload.size()));
    writer.WriteBytes(payload);
    return EncodedDatagram{type, std::move(writer).Finish()};
}

void WriteClientIdentity(
    ByteWriter& writer,
    const MatchId match,
    const PlayerId player,
    const UdpSessionToken& token)
{
    writer.WriteU64(match.value);
    writer.WriteU32(player.value);
    writer.WriteBytes(token);
}

struct ParsedHeader
{
    std::optional<UdpDatagramType> type;
    std::span<const std::byte> payload;
    DecodeError error = DecodeError::None;
};

[[nodiscard]] ParsedHeader ParseHeader(
    const std::span<const std::byte> bytes) noexcept
{
    if (bytes.size() > MaxUdpDatagramBytes)
    {
        return {std::nullopt, {}, DecodeError::CountLimit};
    }
    if (bytes.size() < UdpHeaderBytes)
    {
        return {std::nullopt, {}, DecodeError::Truncated};
    }
    if (!std::equal(UdpMagic.begin(), UdpMagic.end(), bytes.begin()))
    {
        return {std::nullopt, {}, DecodeError::InvalidValue};
    }

    ByteReader reader{bytes.subspan(UdpMagic.size(), UdpHeaderBytes - UdpMagic.size())};
    const auto version = reader.ReadU16();
    const auto typeValue = reader.ReadU8();
    const auto reserved = reader.ReadU8();
    const auto payloadBytes = reader.ReadU16();
    if (!version.has_value()
        || !typeValue.has_value()
        || !reserved.has_value()
        || !payloadBytes.has_value())
    {
        return {std::nullopt, {}, DecodeError::Truncated};
    }
    const auto type = static_cast<UdpDatagramType>(*typeValue);
    if (*version != ProtocolVersion
        || *reserved != 0U
        || !IsKnownDatagramType(type))
    {
        return {std::nullopt, {}, DecodeError::InvalidValue};
    }
    if (*payloadBytes > MaxUdpDatagramBytes - UdpHeaderBytes)
    {
        return {std::nullopt, {}, DecodeError::CountLimit};
    }

    const std::size_t expectedBytes = UdpHeaderBytes + *payloadBytes;
    if (bytes.size() < expectedBytes)
    {
        return {std::nullopt, {}, DecodeError::Truncated};
    }
    if (bytes.size() > expectedBytes)
    {
        return {std::nullopt, {}, DecodeError::TrailingBytes};
    }
    return {
        type,
        bytes.subspan(UdpHeaderBytes, *payloadBytes),
        DecodeError::None};
}

template <typename DatagramVariant>
[[nodiscard]] DatagramDecodeResult<DatagramVariant> Failure(
    const DecodeError error)
{
    return {std::nullopt, error};
}

template <typename DatagramVariant>
[[nodiscard]] DatagramDecodeResult<DatagramVariant> ReaderFailure(
    const ByteReader& reader)
{
    return Failure<DatagramVariant>(
        reader.Error() == DecodeError::None
            ? DecodeError::TrailingBytes
            : reader.Error());
}

template <typename DatagramVariant, typename Datagram>
[[nodiscard]] DatagramDecodeResult<DatagramVariant> FinishDecode(
    const ByteReader& reader,
    Datagram datagram)
{
    if (reader.Error() != DecodeError::None || !reader.Empty())
    {
        return ReaderFailure<DatagramVariant>(reader);
    }
    return {DatagramVariant{std::move(datagram)}, DecodeError::None};
}

struct ClientIdentity
{
    MatchId match;
    PlayerId player;
    UdpSessionToken token;
};

[[nodiscard]] std::optional<ClientIdentity> ReadClientIdentity(
    ByteReader& reader)
{
    const auto match = reader.ReadU64();
    const auto player = reader.ReadU32();
    const auto tokenBytes = reader.ReadBytes(MatchTicketBytes);
    if (!match.has_value() || !player.has_value() || !tokenBytes.has_value())
    {
        return std::nullopt;
    }
    UdpSessionToken token;
    std::copy(tokenBytes->begin(), tokenBytes->end(), token.begin());
    return ClientIdentity{MatchId{*match}, PlayerId{*player}, token};
}

[[nodiscard]] DatagramDecodeResult<ClientDatagram> DecodeBind(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto identity = ReadClientIdentity(reader);
    if (!identity.has_value())
    {
        return ReaderFailure<ClientDatagram>(reader);
    }
    return FinishDecode<ClientDatagram>(
        reader,
        UdpBind{identity->match, identity->player, identity->token});
}

[[nodiscard]] DatagramDecodeResult<ClientDatagram> DecodeInput(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto identity = ReadClientIdentity(reader);
    const auto inputSequence = reader.ReadU32();
    const auto flags = reader.ReadU8();
    if (!identity.has_value()
        || !inputSequence.has_value()
        || !flags.has_value())
    {
        return ReaderFailure<ClientDatagram>(reader);
    }
    if ((*flags & static_cast<std::uint8_t>(~KnownInputFlags)) != 0U)
    {
        return Failure<ClientDatagram>(DecodeError::InvalidValue);
    }

    ClientInput input;
    input.match = identity->match;
    input.player = identity->player;
    input.token = identity->token;
    input.inputSequence = *inputSequence;
    input.hasMoveDestination = (*flags & MoveDestinationFlag) != 0U;
    input.hasAttackTarget = (*flags & AttackTargetFlag) != 0U;
    if (input.hasMoveDestination)
    {
        const auto x = reader.ReadF32();
        const auto z = reader.ReadF32();
        if (!x.has_value() || !z.has_value())
        {
            return ReaderFailure<ClientDatagram>(reader);
        }
        input.moveDestination = {*x, *z};
    }
    if (input.hasAttackTarget)
    {
        const auto target = reader.ReadU32();
        if (!target.has_value())
        {
            return ReaderFailure<ClientDatagram>(reader);
        }
        input.attackTarget = EntityId{*target};
    }
    if (!IsValidInput(input))
    {
        return Failure<ClientDatagram>(DecodeError::InvalidValue);
    }
    return FinishDecode<ClientDatagram>(reader, std::move(input));
}

[[nodiscard]] DatagramDecodeResult<ServerDatagram> DecodeBindAccepted(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto match = reader.ReadU64();
    const auto player = reader.ReadU32();
    const auto serverTick = reader.ReadU32();
    if (!match.has_value() || !player.has_value() || !serverTick.has_value())
    {
        return ReaderFailure<ServerDatagram>(reader);
    }
    return FinishDecode<ServerDatagram>(
        reader,
        UdpBindAccepted{
            MatchId{*match},
            PlayerId{*player},
            *serverTick});
}

[[nodiscard]] DatagramDecodeResult<ServerDatagram> DecodeFragment(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto match = reader.ReadU64();
    const auto snapshotId = reader.ReadU32();
    const auto serverTick = reader.ReadU32();
    const auto ackInputSequence = reader.ReadU32();
    const auto fragmentIndex = reader.ReadU16();
    const auto fragmentCount = reader.ReadU16();
    const auto fullPayloadBytes = reader.ReadU32();
    const auto fullPayloadCrc32 = reader.ReadU32();
    if (!match.has_value()
        || !snapshotId.has_value()
        || !serverTick.has_value()
        || !ackInputSequence.has_value()
        || !fragmentIndex.has_value()
        || !fragmentCount.has_value()
        || !fullPayloadBytes.has_value()
        || !fullPayloadCrc32.has_value())
    {
        return ReaderFailure<ServerDatagram>(reader);
    }
    if (*fragmentCount > MaxSnapshotFragments
        || *fullPayloadBytes > MaxSnapshotPayloadBytes)
    {
        return Failure<ServerDatagram>(DecodeError::CountLimit);
    }

    const auto fragmentBytes = reader.ReadBytes(reader.Remaining());
    if (!fragmentBytes.has_value())
    {
        return ReaderFailure<ServerDatagram>(reader);
    }
    SnapshotFragment fragment{
        MatchId{*match},
        *snapshotId,
        *serverTick,
        *ackInputSequence,
        *fragmentIndex,
        *fragmentCount,
        *fullPayloadBytes,
        *fullPayloadCrc32,
        *fragmentBytes};
    if (!IsValidFragment(fragment))
    {
        return Failure<ServerDatagram>(DecodeError::InvalidValue);
    }
    return FinishDecode<ServerDatagram>(reader, std::move(fragment));
}
} // namespace

EncodedDatagram EncodeClientDatagram(const ClientDatagram& datagram)
{
    return std::visit(
        Overloaded{
            [](const UdpBind& value) {
                const auto payload = EncodePayload([&](ByteWriter& writer) {
                    WriteClientIdentity(
                        writer,
                        value.match,
                        value.player,
                        value.token);
                });
                return EncodeFrame(UdpDatagramType::Bind, payload);
            },
            [](const ClientInput& value) {
                if (!IsValidInput(value))
                {
                    throw std::invalid_argument{"client input is invalid"};
                }
                const auto payload = EncodePayload([&](ByteWriter& writer) {
                    WriteClientIdentity(
                        writer,
                        value.match,
                        value.player,
                        value.token);
                    writer.WriteU32(value.inputSequence);
                    std::uint8_t flags = 0U;
                    if (value.hasMoveDestination)
                    {
                        flags |= MoveDestinationFlag;
                    }
                    if (value.hasAttackTarget)
                    {
                        flags |= AttackTargetFlag;
                    }
                    writer.WriteU8(flags);
                    if (value.hasMoveDestination)
                    {
                        writer.WriteF32(value.moveDestination.x);
                        writer.WriteF32(value.moveDestination.z);
                    }
                    if (value.hasAttackTarget)
                    {
                        writer.WriteU32(value.attackTarget.value);
                    }
                });
                return EncodeFrame(UdpDatagramType::ClientInput, payload);
            }},
        datagram);
}

EncodedDatagram EncodeServerDatagram(const ServerDatagram& datagram)
{
    return std::visit(
        Overloaded{
            [](const UdpBindAccepted& value) {
                const auto payload = EncodePayload([&](ByteWriter& writer) {
                    writer.WriteU64(value.match.value);
                    writer.WriteU32(value.player.value);
                    writer.WriteU32(value.serverTick);
                });
                return EncodeFrame(UdpDatagramType::BindAccepted, payload);
            },
            [](const SnapshotFragment& value) {
                if (!IsValidFragment(value))
                {
                    throw std::invalid_argument{"snapshot fragment is invalid"};
                }
                const auto payload = EncodePayload([&](ByteWriter& writer) {
                    writer.WriteU64(value.match.value);
                    writer.WriteU32(value.snapshotId);
                    writer.WriteU32(value.serverTick);
                    writer.WriteU32(value.ackInputSequence);
                    writer.WriteU16(value.fragmentIndex);
                    writer.WriteU16(value.fragmentCount);
                    writer.WriteU32(value.fullPayloadBytes);
                    writer.WriteU32(value.fullPayloadCrc32);
                    writer.WriteBytes(value.bytes);
                });
                return EncodeFrame(UdpDatagramType::SnapshotFragment, payload);
            }},
        datagram);
}

DatagramDecodeResult<ClientDatagram> DecodeClientDatagram(
    const std::span<const std::byte> bytes)
{
    const ParsedHeader header = ParseHeader(bytes);
    if (!header.type.has_value())
    {
        return Failure<ClientDatagram>(header.error);
    }
    switch (*header.type)
    {
    case UdpDatagramType::Bind:
        return DecodeBind(header.payload);
    case UdpDatagramType::ClientInput:
        return DecodeInput(header.payload);
    default:
        return Failure<ClientDatagram>(DecodeError::InvalidValue);
    }
}

DatagramDecodeResult<ServerDatagram> DecodeServerDatagram(
    const std::span<const std::byte> bytes)
{
    const ParsedHeader header = ParseHeader(bytes);
    if (!header.type.has_value())
    {
        return Failure<ServerDatagram>(header.error);
    }
    switch (*header.type)
    {
    case UdpDatagramType::BindAccepted:
        return DecodeBindAccepted(header.payload);
    case UdpDatagramType::SnapshotFragment:
        return DecodeFragment(header.payload);
    default:
        return Failure<ServerDatagram>(DecodeError::InvalidValue);
    }
}

std::vector<SnapshotFragment> FragmentSnapshot(
    const MatchId match,
    const std::uint32_t snapshotId,
    const std::uint32_t serverTick,
    const std::uint32_t ackInputSequence,
    const std::span<const std::byte> fullPayload)
{
    if (snapshotId == 0U || fullPayload.empty())
    {
        throw std::invalid_argument{"snapshot identity and payload are required"};
    }
    if (fullPayload.size() > MaxSnapshotPayloadBytes)
    {
        throw std::length_error{"snapshot payload exceeds fragment limit"};
    }

    const std::size_t fragmentCount = ExpectedFragmentCount(fullPayload.size());
    const std::uint32_t checksum = Crc32(fullPayload);
    std::vector<SnapshotFragment> fragments;
    fragments.reserve(fragmentCount);
    for (std::size_t index = 0U; index < fragmentCount; ++index)
    {
        const std::size_t offset = index * MaxSnapshotFragmentPayloadBytes;
        const std::size_t count = std::min(
            MaxSnapshotFragmentPayloadBytes,
            fullPayload.size() - offset);
        fragments.push_back(SnapshotFragment{
            match,
            snapshotId,
            serverTick,
            ackInputSequence,
            static_cast<std::uint16_t>(index),
            static_cast<std::uint16_t>(fragmentCount),
            static_cast<std::uint32_t>(fullPayload.size()),
            checksum,
            std::vector<std::byte>{
                fullPayload.begin() + static_cast<std::ptrdiff_t>(offset),
                fullPayload.begin() + static_cast<std::ptrdiff_t>(offset + count)}});
    }
    return fragments;
}
} // namespace dxa::protocol
