#include <dxa/protocol/WorkerControlMessageCodec.hpp>

#include <dxa/protocol/ByteCodec.hpp>

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dxa::protocol
{
namespace
{
constexpr std::uint32_t MaximumTicketLifetimeMilliseconds =
    static_cast<std::uint32_t>(MatchTicketLifetimeSeconds * 1000U);

template <typename... Functions>
struct Overloaded : Functions...
{
    using Functions::operator()...;
};

template <typename... Functions>
Overloaded(Functions...) -> Overloaded<Functions...>;

[[nodiscard]] bool IsVisibleAsciiHost(const std::string_view host) noexcept
{
    if (host.empty() || host.size() > 255U)
    {
        return false;
    }
    return std::all_of(host.begin(), host.end(), [](const char character) {
        const auto value = static_cast<unsigned char>(character);
        return value >= 0x21U && value <= 0x7EU;
    });
}

[[nodiscard]] bool IsValidRegistration(const WorkerRegister& value) noexcept
{
    return value.worker.value != 0U
        && IsVisibleAsciiHost(value.advertisedHost)
        && value.gameTcpPort != 0U
        && value.gameUdpPort != 0U
        && value.capacity == 1U;
}

[[nodiscard]] bool IsValidReservationId(const ReservationId value) noexcept
{
    return value.value != 0U;
}

[[nodiscard]] bool IsValidRejectReason(
    const WorkerReservationReject reason) noexcept
{
    switch (reason)
    {
    case WorkerReservationReject::Busy:
    case WorkerReservationReject::InvalidReservation:
    case WorkerReservationReject::SimulationInitializationFailed:
    case WorkerReservationReject::InternalError:
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

[[nodiscard]] bool IsValidCompletion(const MatchFinished& value) noexcept
{
    return IsValidCompletionReason(value.reason)
        && value.hasWinner == CompletionNeedsWinner(value.reason);
}

[[nodiscard]] bool CanonicalizeParticipants(
    std::vector<ReservedParticipant>& participants)
{
    if (participants.size() < 2U || participants.size() > RoomCapacity)
    {
        return false;
    }
    std::sort(
        participants.begin(),
        participants.end(),
        [](const ReservedParticipant& left, const ReservedParticipant& right) {
            return left.player < right.player;
        });
    return std::adjacent_find(
               participants.begin(),
               participants.end(),
               [](const ReservedParticipant& left,
                  const ReservedParticipant& right) {
                   return left.player == right.player;
               }) == participants.end();
}

[[nodiscard]] bool IsValidReservation(ReserveMatch& value)
{
    return IsValidReservationId(value.reservation)
        && value.ticketLifetimeMilliseconds > 0U
        && value.ticketLifetimeMilliseconds
            <= MaximumTicketLifetimeMilliseconds
        && CanonicalizeParticipants(value.participants);
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

void WriteReservationIdentity(
    ByteWriter& writer,
    const ReservationId reservation,
    const MatchId match)
{
    writer.WriteU64(reservation.value);
    writer.WriteU64(match.value);
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

[[nodiscard]] MessageDecodeResult<WorkerToLobbyMessage> DecodeRegistration(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto worker = reader.ReadU32();
    const auto host = reader.ReadString8(255U);
    const auto tcpPort = reader.ReadU16();
    const auto udpPort = reader.ReadU16();
    const auto capacity = reader.ReadU8();
    if (!worker.has_value()
        || !host.has_value()
        || !tcpPort.has_value()
        || !udpPort.has_value()
        || !capacity.has_value())
    {
        return ReaderFailure<WorkerToLobbyMessage>(reader);
    }

    WorkerRegister message{
        WorkerId{*worker},
        *host,
        *tcpPort,
        *udpPort,
        *capacity};
    if (!IsValidRegistration(message))
    {
        return Failure<WorkerToLobbyMessage>(DecodeError::InvalidValue);
    }
    return FinishDecode<WorkerToLobbyMessage>(reader, std::move(message));
}

template <typename Message>
[[nodiscard]] MessageDecodeResult<WorkerToLobbyMessage>
DecodeWorkerReservationIdentity(const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto reservation = reader.ReadU64();
    const auto match = reader.ReadU64();
    if (!reservation.has_value() || !match.has_value())
    {
        return ReaderFailure<WorkerToLobbyMessage>(reader);
    }
    if (*reservation == 0U)
    {
        return Failure<WorkerToLobbyMessage>(DecodeError::InvalidValue);
    }
    return FinishDecode<WorkerToLobbyMessage>(
        reader,
        Message{ReservationId{*reservation}, MatchId{*match}});
}

[[nodiscard]] MessageDecodeResult<WorkerToLobbyMessage> DecodeReservationReject(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto reservation = reader.ReadU64();
    const auto match = reader.ReadU64();
    const auto reasonValue = reader.ReadU8();
    if (!reservation.has_value()
        || !match.has_value()
        || !reasonValue.has_value())
    {
        return ReaderFailure<WorkerToLobbyMessage>(reader);
    }
    const auto reason = static_cast<WorkerReservationReject>(*reasonValue);
    if (*reservation == 0U || !IsValidRejectReason(reason))
    {
        return Failure<WorkerToLobbyMessage>(DecodeError::InvalidValue);
    }
    return FinishDecode<WorkerToLobbyMessage>(
        reader,
        ReserveMatchRejected{
            ReservationId{*reservation},
            MatchId{*match},
            reason});
}

[[nodiscard]] MessageDecodeResult<WorkerToLobbyMessage> DecodeMatchFinished(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto match = reader.ReadU64();
    const auto hasWinner = reader.ReadU8();
    if (!match.has_value() || !hasWinner.has_value())
    {
        return ReaderFailure<WorkerToLobbyMessage>(reader);
    }
    if (*hasWinner > 1U)
    {
        return Failure<WorkerToLobbyMessage>(DecodeError::InvalidValue);
    }

    EntityId winner;
    if (*hasWinner == 1U)
    {
        const auto winnerValue = reader.ReadU32();
        if (!winnerValue.has_value())
        {
            return ReaderFailure<WorkerToLobbyMessage>(reader);
        }
        winner = EntityId{*winnerValue};
    }
    const auto reasonValue = reader.ReadU8();
    const auto finishedTick = reader.ReadU32();
    if (!reasonValue.has_value() || !finishedTick.has_value())
    {
        return ReaderFailure<WorkerToLobbyMessage>(reader);
    }

    MatchFinished message{
        MatchId{*match},
        winner,
        *hasWinner == 1U,
        static_cast<MatchCompletionReason>(*reasonValue),
        *finishedTick};
    if (!IsValidCompletion(message))
    {
        return Failure<WorkerToLobbyMessage>(DecodeError::InvalidValue);
    }
    return FinishDecode<WorkerToLobbyMessage>(reader, std::move(message));
}

[[nodiscard]] MessageDecodeResult<LobbyToWorkerMessage> DecodeWorkerRegistered(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto worker = reader.ReadU32();
    if (!worker.has_value())
    {
        return ReaderFailure<LobbyToWorkerMessage>(reader);
    }
    if (*worker == 0U)
    {
        return Failure<LobbyToWorkerMessage>(DecodeError::InvalidValue);
    }
    return FinishDecode<LobbyToWorkerMessage>(
        reader,
        WorkerRegistered{WorkerId{*worker}});
}

[[nodiscard]] MessageDecodeResult<LobbyToWorkerMessage> DecodeReservation(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto reservation = reader.ReadU64();
    const auto match = reader.ReadU64();
    const auto seed = reader.ReadU32();
    const auto lifetime = reader.ReadU32();
    const auto count = reader.ReadU8();
    if (!reservation.has_value()
        || !match.has_value()
        || !seed.has_value()
        || !lifetime.has_value()
        || !count.has_value())
    {
        return ReaderFailure<LobbyToWorkerMessage>(reader);
    }
    if (*count > RoomCapacity)
    {
        return Failure<LobbyToWorkerMessage>(DecodeError::CountLimit);
    }
    if (*count < 2U)
    {
        return Failure<LobbyToWorkerMessage>(DecodeError::InvalidValue);
    }

    std::vector<ReservedParticipant> participants;
    participants.reserve(*count);
    for (std::uint8_t index = 0U; index < *count; ++index)
    {
        const auto player = reader.ReadU32();
        const auto ticketBytes = reader.ReadBytes(MatchTicketBytes);
        if (!player.has_value() || !ticketBytes.has_value())
        {
            return ReaderFailure<LobbyToWorkerMessage>(reader);
        }
        MatchTicketValue ticket;
        std::copy(ticketBytes->begin(), ticketBytes->end(), ticket.begin());
        participants.push_back({PlayerId{*player}, ticket});
    }

    ReserveMatch message{
        ReservationId{*reservation},
        MatchId{*match},
        *seed,
        *lifetime,
        std::move(participants)};
    if (!IsValidReservation(message))
    {
        return Failure<LobbyToWorkerMessage>(DecodeError::InvalidValue);
    }
    return FinishDecode<LobbyToWorkerMessage>(reader, std::move(message));
}

[[nodiscard]] MessageDecodeResult<LobbyToWorkerMessage> DecodeCancelReservation(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto reservation = reader.ReadU64();
    const auto match = reader.ReadU64();
    if (!reservation.has_value() || !match.has_value())
    {
        return ReaderFailure<LobbyToWorkerMessage>(reader);
    }
    if (*reservation == 0U)
    {
        return Failure<LobbyToWorkerMessage>(DecodeError::InvalidValue);
    }
    return FinishDecode<LobbyToWorkerMessage>(
        reader,
        CancelMatchReservation{
            ReservationId{*reservation},
            MatchId{*match}});
}
} // namespace

EncodedMessage EncodeWorkerToLobbyMessage(const WorkerToLobbyMessage& message)
{
    return std::visit(
        Overloaded{
            [](const WorkerRegister& value) {
                if (!IsValidRegistration(value))
                {
                    throw std::invalid_argument{"worker registration is invalid"};
                }
                return EncodePayload(MessageType::WorkerRegister, [&](ByteWriter& writer) {
                    writer.WriteU32(value.worker.value);
                    writer.WriteString8(value.advertisedHost);
                    writer.WriteU16(value.gameTcpPort);
                    writer.WriteU16(value.gameUdpPort);
                    writer.WriteU8(value.capacity);
                });
            },
            [](const ReserveMatchReady& value) {
                if (!IsValidReservationId(value.reservation))
                {
                    throw std::invalid_argument{"reservation ready identity is invalid"};
                }
                return EncodePayload(MessageType::ReserveMatchReady, [&](ByteWriter& writer) {
                    WriteReservationIdentity(writer, value.reservation, value.match);
                });
            },
            [](const ReserveMatchRejected& value) {
                if (!IsValidReservationId(value.reservation)
                    || !IsValidRejectReason(value.reason))
                {
                    throw std::invalid_argument{"reservation rejection is invalid"};
                }
                return EncodePayload(MessageType::ReserveMatchRejected, [&](ByteWriter& writer) {
                    WriteReservationIdentity(writer, value.reservation, value.match);
                    writer.WriteU8(static_cast<std::uint8_t>(value.reason));
                });
            },
            [](const MatchReservationCancelled& value) {
                if (!IsValidReservationId(value.reservation))
                {
                    throw std::invalid_argument{"reservation cancellation is invalid"};
                }
                return EncodePayload(
                    MessageType::MatchReservationCancelled,
                    [&](ByteWriter& writer) {
                        WriteReservationIdentity(writer, value.reservation, value.match);
                    });
            },
            [](const MatchFinished& value) {
                if (!IsValidCompletion(value))
                {
                    throw std::invalid_argument{"match completion is invalid"};
                }
                return EncodePayload(MessageType::MatchFinished, [&](ByteWriter& writer) {
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

EncodedMessage EncodeLobbyToWorkerMessage(const LobbyToWorkerMessage& message)
{
    return std::visit(
        Overloaded{
            [](const WorkerRegistered& value) {
                if (value.worker.value == 0U)
                {
                    throw std::invalid_argument{"registered worker identity is invalid"};
                }
                return EncodePayload(MessageType::WorkerRegistered, [&](ByteWriter& writer) {
                    writer.WriteU32(value.worker.value);
                });
            },
            [](ReserveMatch value) {
                if (!IsValidReservation(value))
                {
                    throw std::invalid_argument{"match reservation is invalid"};
                }
                return EncodePayload(MessageType::ReserveMatch, [&](ByteWriter& writer) {
                    WriteReservationIdentity(writer, value.reservation, value.match);
                    writer.WriteU32(value.seed);
                    writer.WriteU32(value.ticketLifetimeMilliseconds);
                    writer.WriteU8(static_cast<std::uint8_t>(value.participants.size()));
                    for (const ReservedParticipant& participant : value.participants)
                    {
                        writer.WriteU32(participant.player.value);
                        writer.WriteBytes(participant.ticket);
                    }
                });
            },
            [](const CancelMatchReservation& value) {
                if (!IsValidReservationId(value.reservation))
                {
                    throw std::invalid_argument{"match reservation cancellation is invalid"};
                }
                return EncodePayload(
                    MessageType::CancelMatchReservation,
                    [&](ByteWriter& writer) {
                        WriteReservationIdentity(writer, value.reservation, value.match);
                    });
            }},
        message);
}

MessageDecodeResult<WorkerToLobbyMessage> DecodeWorkerToLobbyMessage(
    const MessageType type,
    const std::span<const std::byte> payload)
{
    switch (type)
    {
    case MessageType::WorkerRegister:
        return DecodeRegistration(payload);
    case MessageType::ReserveMatchReady:
        return DecodeWorkerReservationIdentity<ReserveMatchReady>(payload);
    case MessageType::ReserveMatchRejected:
        return DecodeReservationReject(payload);
    case MessageType::MatchReservationCancelled:
        return DecodeWorkerReservationIdentity<MatchReservationCancelled>(payload);
    case MessageType::MatchFinished:
        return DecodeMatchFinished(payload);
    default:
        return Failure<WorkerToLobbyMessage>(DecodeError::InvalidValue);
    }
}

MessageDecodeResult<LobbyToWorkerMessage> DecodeLobbyToWorkerMessage(
    const MessageType type,
    const std::span<const std::byte> payload)
{
    switch (type)
    {
    case MessageType::WorkerRegistered:
        return DecodeWorkerRegistered(payload);
    case MessageType::ReserveMatch:
        return DecodeReservation(payload);
    case MessageType::CancelMatchReservation:
        return DecodeCancelReservation(payload);
    default:
        return Failure<LobbyToWorkerMessage>(DecodeError::InvalidValue);
    }
}
} // namespace dxa::protocol
