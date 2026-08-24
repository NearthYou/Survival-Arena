#include <dxa/protocol/GameTcpMessageCodec.hpp>

#include <dxa/protocol/ByteCodec.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dxa::protocol
{
namespace
{
template <typename... Functions>
struct Overloaded : Functions...
{
    using Functions::operator()...;
};

template <typename... Functions>
Overloaded(Functions...) -> Overloaded<Functions...>;

[[nodiscard]] bool IsValidErrorCode(const GameServerErrorCode error) noexcept
{
    switch (error)
    {
    case GameServerErrorCode::AuthenticationFailed:
    case GameServerErrorCode::ServerNotReady:
    case GameServerErrorCode::ProtocolViolation:
    case GameServerErrorCode::InternalError:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidCompletionReason(
    const MatchCompletionReason reason) noexcept
{
    switch (reason)
    {
    case MatchCompletionReason::LastSurvivor:
    case MatchCompletionReason::TimeLimit:
    case MatchCompletionReason::NoAuthenticatedPlayers:
    case MatchCompletionReason::NoConnectedPlayers:
        return true;
    }
    return false;
}

[[nodiscard]] bool CompletionNeedsWinner(
    const MatchCompletionReason reason) noexcept
{
    return reason == MatchCompletionReason::LastSurvivor
        || reason == MatchCompletionReason::TimeLimit;
}

[[nodiscard]] bool IsValidResult(const GameMatchResult& value) noexcept
{
    return IsValidCompletionReason(value.reason)
        && value.hasWinner == CompletionNeedsWinner(value.reason);
}

[[nodiscard]] bool IsValidWelcome(const GameServerWelcome& value) noexcept
{
    return value.tickRate == GameTickRate
        && value.snapshotRate == SnapshotRate
        && value.mapId != 0U;
}

template <typename Writer>
[[nodiscard]] EncodedMessage EncodePayload(
    const MessageType type,
    Writer write)
{
    ByteWriter writer;
    write(writer);
    return EncodedMessage{type, std::move(writer).Finish()};
}

template <typename MessageVariant>
[[nodiscard]] MessageDecodeResult<MessageVariant> Failure(
    const DecodeError error)
{
    return {std::nullopt, error};
}

template <typename MessageVariant>
[[nodiscard]] MessageDecodeResult<MessageVariant> ReaderFailure(
    const ByteReader& reader)
{
    return Failure<MessageVariant>(
        reader.Error() == DecodeError::None
            ? DecodeError::TrailingBytes
            : reader.Error());
}

template <typename MessageVariant, typename Message>
[[nodiscard]] MessageDecodeResult<MessageVariant> FinishDecode(
    const ByteReader& reader,
    Message message)
{
    if (reader.Error() != DecodeError::None || !reader.Empty())
    {
        return ReaderFailure<MessageVariant>(reader);
    }
    return {MessageVariant{std::move(message)}, DecodeError::None};
}

[[nodiscard]] GameClientHello ReadHello(
    const std::uint64_t match,
    const std::uint32_t player,
    const std::vector<std::byte>& ticketBytes)
{
    MatchTicketValue ticket;
    std::copy(ticketBytes.begin(), ticketBytes.end(), ticket.begin());
    return GameClientHello{MatchId{match}, PlayerId{player}, ticket};
}

[[nodiscard]] MessageDecodeResult<GameClientMessage> DecodeHello(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto match = reader.ReadU64();
    const auto player = reader.ReadU32();
    const auto ticket = reader.ReadBytes(MatchTicketBytes);
    if (!match.has_value() || !player.has_value() || !ticket.has_value())
    {
        return ReaderFailure<GameClientMessage>(reader);
    }
    return FinishDecode<GameClientMessage>(
        reader,
        ReadHello(*match, *player, *ticket));
}

[[nodiscard]] MessageDecodeResult<GameServerMessage> DecodeWelcome(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto match = reader.ReadU64();
    const auto player = reader.ReadU32();
    const auto actor = reader.ReadU32();
    const auto tickRate = reader.ReadU16();
    const auto snapshotRate = reader.ReadU16();
    const auto mapId = reader.ReadU32();
    const auto navMeshCrc32 = reader.ReadU32();
    const auto tokenBytes = reader.ReadBytes(MatchTicketBytes);
    if (!match.has_value()
        || !player.has_value()
        || !actor.has_value()
        || !tickRate.has_value()
        || !snapshotRate.has_value()
        || !mapId.has_value()
        || !navMeshCrc32.has_value()
        || !tokenBytes.has_value())
    {
        return ReaderFailure<GameServerMessage>(reader);
    }

    UdpSessionToken token;
    std::copy(tokenBytes->begin(), tokenBytes->end(), token.begin());
    GameServerWelcome message{
        MatchId{*match},
        PlayerId{*player},
        EntityId{*actor},
        *tickRate,
        *snapshotRate,
        *mapId,
        *navMeshCrc32,
        token};
    if (!IsValidWelcome(message))
    {
        return Failure<GameServerMessage>(DecodeError::InvalidValue);
    }
    return FinishDecode<GameServerMessage>(reader, std::move(message));
}

[[nodiscard]] MessageDecodeResult<GameServerMessage> DecodeErrorMessage(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto errorValue = reader.ReadU8();
    if (!errorValue.has_value())
    {
        return ReaderFailure<GameServerMessage>(reader);
    }
    const auto error = static_cast<GameServerErrorCode>(*errorValue);
    if (!IsValidErrorCode(error))
    {
        return Failure<GameServerMessage>(DecodeError::InvalidValue);
    }
    return FinishDecode<GameServerMessage>(
        reader,
        GameServerErrorMessage{error});
}

[[nodiscard]] MessageDecodeResult<GameServerMessage> DecodeResult(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto match = reader.ReadU64();
    const auto hasWinner = reader.ReadU8();
    if (!match.has_value() || !hasWinner.has_value())
    {
        return ReaderFailure<GameServerMessage>(reader);
    }
    if (*hasWinner > 1U)
    {
        return Failure<GameServerMessage>(DecodeError::InvalidValue);
    }

    EntityId winner;
    if (*hasWinner == 1U)
    {
        const auto winnerValue = reader.ReadU32();
        if (!winnerValue.has_value())
        {
            return ReaderFailure<GameServerMessage>(reader);
        }
        winner = EntityId{*winnerValue};
    }
    const auto reasonValue = reader.ReadU8();
    const auto finishedTick = reader.ReadU32();
    if (!reasonValue.has_value() || !finishedTick.has_value())
    {
        return ReaderFailure<GameServerMessage>(reader);
    }

    GameMatchResult message{
        MatchId{*match},
        winner,
        *hasWinner == 1U,
        static_cast<MatchCompletionReason>(*reasonValue),
        *finishedTick};
    if (!IsValidResult(message))
    {
        return Failure<GameServerMessage>(DecodeError::InvalidValue);
    }
    return FinishDecode<GameServerMessage>(reader, std::move(message));
}
} // namespace

EncodedMessage EncodeGameClientMessage(const GameClientMessage& message)
{
    return std::visit(
        [](const GameClientHello& value) {
            return EncodePayload(MessageType::GameClientHello, [&](ByteWriter& writer) {
                writer.WriteU64(value.match.value);
                writer.WriteU32(value.player.value);
                writer.WriteBytes(value.ticket);
            });
        },
        message);
}

EncodedMessage EncodeGameServerMessage(const GameServerMessage& message)
{
    return std::visit(
        Overloaded{
            [](const GameServerWelcome& value) {
                if (!IsValidWelcome(value))
                {
                    throw std::invalid_argument{"game welcome is invalid"};
                }
                return EncodePayload(MessageType::GameServerWelcome, [&](ByteWriter& writer) {
                    writer.WriteU64(value.match.value);
                    writer.WriteU32(value.player.value);
                    writer.WriteU32(value.actor.value);
                    writer.WriteU16(value.tickRate);
                    writer.WriteU16(value.snapshotRate);
                    writer.WriteU32(value.mapId);
                    writer.WriteU32(value.navMeshCrc32);
                    writer.WriteBytes(value.udpToken);
                });
            },
            [](const GameServerErrorMessage& value) {
                if (!IsValidErrorCode(value.error))
                {
                    throw std::invalid_argument{"game server error is invalid"};
                }
                return EncodePayload(MessageType::GameServerError, [&](ByteWriter& writer) {
                    writer.WriteU8(static_cast<std::uint8_t>(value.error));
                });
            },
            [](const GameMatchResult& value) {
                if (!IsValidResult(value))
                {
                    throw std::invalid_argument{"game match result is invalid"};
                }
                return EncodePayload(MessageType::GameMatchResult, [&](ByteWriter& writer) {
                    writer.WriteU64(value.match.value);
                    writer.WriteU8(value.hasWinner ? 1U : 0U);
                    if (value.hasWinner)
                    {
                        writer.WriteU32(value.winner.value);
                    }
                    writer.WriteU8(static_cast<std::uint8_t>(value.reason));
                    writer.WriteU32(value.finishedTick);
                });
            }},
        message);
}

MessageDecodeResult<GameClientMessage> DecodeGameClientMessage(
    const MessageType type,
    const std::span<const std::byte> payload)
{
    if (type != MessageType::GameClientHello)
    {
        return Failure<GameClientMessage>(DecodeError::InvalidValue);
    }
    return DecodeHello(payload);
}

MessageDecodeResult<GameServerMessage> DecodeGameServerMessage(
    const MessageType type,
    const std::span<const std::byte> payload)
{
    switch (type)
    {
    case MessageType::GameServerWelcome:
        return DecodeWelcome(payload);
    case MessageType::GameServerError:
        return DecodeErrorMessage(payload);
    case MessageType::GameMatchResult:
        return DecodeResult(payload);
    default:
        return Failure<GameServerMessage>(DecodeError::InvalidValue);
    }
}
} // namespace dxa::protocol
