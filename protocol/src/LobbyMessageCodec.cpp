#include <dxa/protocol/LobbyMessageCodec.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <type_traits>
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

[[nodiscard]] bool IsValidRoomState(const RoomState state) noexcept
{
    switch (state)
    {
    case RoomState::Waiting:
    case RoomState::Starting:
    case RoomState::InMatch:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidLobbyError(const LobbyError error) noexcept
{
    const auto value = static_cast<std::uint16_t>(error);
    return value >= static_cast<std::uint16_t>(LobbyError::MalformedPayload)
        && value <= static_cast<std::uint16_t>(LobbyError::MatchUnavailable);
}

[[nodiscard]] bool IsValidHost(const std::string_view host) noexcept
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

[[nodiscard]] bool IsValidTicket(const MatchTicket& ticket) noexcept
{
    return IsValidHost(ticket.host)
        && ticket.tcpPort != 0U
        && ticket.udpPort != 0U
        && ticket.expiresInSeconds > 0U
        && ticket.expiresInSeconds <= MatchTicketLifetimeSeconds;
}

template <typename BodyWriter>
[[nodiscard]] EncodedMessage EncodePayload(
    const MessageType type,
    const std::uint32_t requestId,
    BodyWriter writeBody)
{
    ByteWriter writer;
    writer.WriteU32(requestId);
    writeBody(writer);
    return EncodedMessage{type, std::move(writer).Finish()};
}

template <typename MessageVariant>
[[nodiscard]] MessageDecodeResult<MessageVariant> Failure(const DecodeError error)
{
    return {std::nullopt, error};
}

template <typename MessageVariant>
[[nodiscard]] MessageDecodeResult<MessageVariant> ReaderFailure(const ByteReader& reader)
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

[[nodiscard]] std::optional<std::uint32_t> ReadClientRequestId(ByteReader& reader)
{
    const auto requestId = reader.ReadU32();
    if (!requestId.has_value() || *requestId == 0U)
    {
        return std::nullopt;
    }
    return requestId;
}

template <typename Message>
[[nodiscard]] MessageDecodeResult<ClientMessage> DecodeRequestOnly(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto requestId = ReadClientRequestId(reader);
    if (!requestId.has_value())
    {
        return Failure<ClientMessage>(
            reader.Error() == DecodeError::None
                ? DecodeError::InvalidValue
                : reader.Error());
    }
    return FinishDecode<ClientMessage>(reader, Message{*requestId});
}

[[nodiscard]] MessageDecodeResult<ClientMessage> DecodeJoinRoom(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto requestId = ReadClientRequestId(reader);
    const auto room = reader.ReadU32();
    if (!requestId.has_value() || !room.has_value())
    {
        return Failure<ClientMessage>(
            reader.Error() == DecodeError::None
                ? DecodeError::InvalidValue
                : reader.Error());
    }
    return FinishDecode<ClientMessage>(
        reader,
        JoinRoomRequest{*requestId, RoomId{*room}});
}

[[nodiscard]] MessageDecodeResult<ClientMessage> DecodeSetReady(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto requestId = ReadClientRequestId(reader);
    const auto ready = reader.ReadU8();
    if (!requestId.has_value() || !ready.has_value())
    {
        return Failure<ClientMessage>(
            reader.Error() == DecodeError::None
                ? DecodeError::InvalidValue
                : reader.Error());
    }
    if (*ready > 1U)
    {
        return Failure<ClientMessage>(DecodeError::InvalidValue);
    }
    return FinishDecode<ClientMessage>(
        reader,
        SetReadyRequest{*requestId, *ready == 1U});
}

[[nodiscard]] MessageDecodeResult<ServerMessage> DecodeWelcome(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto requestId = reader.ReadU32();
    const auto player = reader.ReadU32();
    if (!requestId.has_value() || !player.has_value())
    {
        return ReaderFailure<ServerMessage>(reader);
    }
    return FinishDecode<ServerMessage>(
        reader,
        ServerWelcome{*requestId, PlayerId{*player}});
}

[[nodiscard]] MessageDecodeResult<ServerMessage> DecodeRoomList(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto requestId = reader.ReadU32();
    const auto count = reader.ReadU16();
    if (!requestId.has_value() || !count.has_value())
    {
        return ReaderFailure<ServerMessage>(reader);
    }
    if (*count > MaximumRooms)
    {
        return Failure<ServerMessage>(DecodeError::CountLimit);
    }

    std::vector<RoomSummary> rooms;
    rooms.reserve(*count);
    for (std::uint16_t index = 0; index < *count; ++index)
    {
        const auto room = reader.ReadU32();
        const auto players = reader.ReadU8();
        const auto capacity = reader.ReadU8();
        if (!room.has_value() || !players.has_value() || !capacity.has_value())
        {
            return ReaderFailure<ServerMessage>(reader);
        }
        if (*capacity != RoomCapacity || *players > *capacity)
        {
            return Failure<ServerMessage>(DecodeError::InvalidValue);
        }
        rooms.push_back(RoomSummary{RoomId{*room}, *players, *capacity});
    }
    std::sort(rooms.begin(), rooms.end(), [](const RoomSummary& left, const RoomSummary& right) {
        return left.room < right.room;
    });
    return FinishDecode<ServerMessage>(
        reader,
        RoomListResponse{*requestId, std::move(rooms)});
}

[[nodiscard]] MessageDecodeResult<ServerMessage> DecodeRoomSnapshot(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto requestId = reader.ReadU32();
    const auto room = reader.ReadU32();
    const auto stateValue = reader.ReadU8();
    const auto host = reader.ReadU32();
    const auto count = reader.ReadU8();
    if (!requestId.has_value()
        || !room.has_value()
        || !stateValue.has_value()
        || !host.has_value()
        || !count.has_value())
    {
        return ReaderFailure<ServerMessage>(reader);
    }
    const auto state = static_cast<RoomState>(*stateValue);
    if (!IsValidRoomState(state))
    {
        return Failure<ServerMessage>(DecodeError::InvalidValue);
    }
    if (*count > RoomCapacity)
    {
        return Failure<ServerMessage>(DecodeError::CountLimit);
    }

    std::vector<RoomMemberView> members;
    members.reserve(*count);
    for (std::uint8_t index = 0; index < *count; ++index)
    {
        const auto player = reader.ReadU32();
        const auto ready = reader.ReadU8();
        if (!player.has_value() || !ready.has_value())
        {
            return ReaderFailure<ServerMessage>(reader);
        }
        if (*ready > 1U)
        {
            return Failure<ServerMessage>(DecodeError::InvalidValue);
        }
        members.push_back(RoomMemberView{PlayerId{*player}, *ready == 1U});
    }
    std::sort(
        members.begin(),
        members.end(),
        [](const RoomMemberView& left, const RoomMemberView& right) {
            return left.player < right.player;
        });
    return FinishDecode<ServerMessage>(
        reader,
        RoomSnapshot{
            *requestId,
            RoomId{*room},
            state,
            PlayerId{*host},
            std::move(members)});
}

[[nodiscard]] MessageDecodeResult<ServerMessage> DecodeMatchTicket(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto requestId = reader.ReadU32();
    const auto match = reader.ReadU64();
    const auto ticketBytes = reader.ReadBytes(MatchTicketBytes);
    const auto host = reader.ReadString8(255U);
    const auto tcpPort = reader.ReadU16();
    const auto udpPort = reader.ReadU16();
    const auto expires = reader.ReadU16();
    if (!requestId.has_value()
        || !match.has_value()
        || !ticketBytes.has_value()
        || !host.has_value()
        || !tcpPort.has_value()
        || !udpPort.has_value()
        || !expires.has_value())
    {
        return ReaderFailure<ServerMessage>(reader);
    }

    MatchTicket ticket;
    ticket.requestId = *requestId;
    ticket.match = MatchId{*match};
    std::copy(ticketBytes->begin(), ticketBytes->end(), ticket.ticket.begin());
    ticket.host = *host;
    ticket.tcpPort = *tcpPort;
    ticket.udpPort = *udpPort;
    ticket.expiresInSeconds = *expires;
    if (!IsValidTicket(ticket))
    {
        return Failure<ServerMessage>(DecodeError::InvalidValue);
    }
    return FinishDecode<ServerMessage>(reader, std::move(ticket));
}

[[nodiscard]] MessageDecodeResult<ServerMessage> DecodeErrorResponse(
    const std::span<const std::byte> payload)
{
    ByteReader reader{payload};
    const auto requestId = reader.ReadU32();
    const auto errorValue = reader.ReadU16();
    if (!requestId.has_value() || !errorValue.has_value())
    {
        return ReaderFailure<ServerMessage>(reader);
    }
    const auto error = static_cast<LobbyError>(*errorValue);
    if (!IsValidLobbyError(error))
    {
        return Failure<ServerMessage>(DecodeError::InvalidValue);
    }
    return FinishDecode<ServerMessage>(
        reader,
        ErrorResponse{*requestId, error});
}
} // namespace

std::uint32_t RequestId(const ClientMessage& message) noexcept
{
    return std::visit(
        [](const auto& value) { return value.requestId; },
        message);
}

std::uint32_t RequestId(const ServerMessage& message) noexcept
{
    return std::visit(
        [](const auto& value) { return value.requestId; },
        message);
}

EncodedMessage EncodeClientMessage(const ClientMessage& message)
{
    const std::uint32_t requestId = RequestId(message);
    if (requestId == 0U)
    {
        throw std::invalid_argument{"client request ID zero is reserved for server push"};
    }

    return std::visit(
        Overloaded{
            [requestId](const ClientHello&) {
                return EncodePayload(MessageType::ClientHello, requestId, [](ByteWriter&) {});
            },
            [requestId](const ListRoomsRequest&) {
                return EncodePayload(MessageType::ListRoomsRequest, requestId, [](ByteWriter&) {});
            },
            [requestId](const CreateRoomRequest&) {
                return EncodePayload(MessageType::CreateRoomRequest, requestId, [](ByteWriter&) {});
            },
            [requestId](const JoinRoomRequest& value) {
                return EncodePayload(MessageType::JoinRoomRequest, requestId, [&](ByteWriter& writer) {
                    writer.WriteU32(value.room.value);
                });
            },
            [requestId](const LeaveRoomRequest&) {
                return EncodePayload(MessageType::LeaveRoomRequest, requestId, [](ByteWriter&) {});
            },
            [requestId](const SetReadyRequest& value) {
                return EncodePayload(MessageType::SetReadyRequest, requestId, [&](ByteWriter& writer) {
                    writer.WriteU8(value.ready ? 1U : 0U);
                });
            },
            [requestId](const StartMatchRequest&) {
                return EncodePayload(MessageType::StartMatchRequest, requestId, [](ByteWriter&) {});
            }},
        message);
}

EncodedMessage EncodeServerMessage(const ServerMessage& message)
{
    return std::visit(
        Overloaded{
            [](const ServerWelcome& value) {
                return EncodePayload(MessageType::ServerWelcome, value.requestId, [&](ByteWriter& writer) {
                    writer.WriteU32(value.player.value);
                });
            },
            [](const RoomListResponse& value) {
                if (value.rooms.size() > MaximumRooms)
                {
                    throw std::invalid_argument{"room list exceeds protocol limit"};
                }
                std::vector<RoomSummary> rooms = value.rooms;
                for (const RoomSummary& room : rooms)
                {
                    if (room.capacity != RoomCapacity || room.players > room.capacity)
                    {
                        throw std::invalid_argument{"room summary is invalid"};
                    }
                }
                std::sort(rooms.begin(), rooms.end(), [](const RoomSummary& left, const RoomSummary& right) {
                    return left.room < right.room;
                });
                return EncodePayload(MessageType::RoomListResponse, value.requestId, [&](ByteWriter& writer) {
                    writer.WriteU16(static_cast<std::uint16_t>(rooms.size()));
                    for (const RoomSummary& room : rooms)
                    {
                        writer.WriteU32(room.room.value);
                        writer.WriteU8(room.players);
                        writer.WriteU8(room.capacity);
                    }
                });
            },
            [](const RoomSnapshot& value) {
                if (!IsValidRoomState(value.state) || value.members.size() > RoomCapacity)
                {
                    throw std::invalid_argument{"room snapshot is invalid"};
                }
                std::vector<RoomMemberView> members = value.members;
                std::sort(
                    members.begin(),
                    members.end(),
                    [](const RoomMemberView& left, const RoomMemberView& right) {
                        return left.player < right.player;
                    });
                return EncodePayload(MessageType::RoomSnapshot, value.requestId, [&](ByteWriter& writer) {
                    writer.WriteU32(value.room.value);
                    writer.WriteU8(static_cast<std::uint8_t>(value.state));
                    writer.WriteU32(value.host.value);
                    writer.WriteU8(static_cast<std::uint8_t>(members.size()));
                    for (const RoomMemberView& member : members)
                    {
                        writer.WriteU32(member.player.value);
                        writer.WriteU8(member.ready ? 1U : 0U);
                    }
                });
            },
            [](const MatchTicket& value) {
                if (!IsValidTicket(value))
                {
                    throw std::invalid_argument{"match ticket endpoint is invalid"};
                }
                return EncodePayload(MessageType::MatchTicket, value.requestId, [&](ByteWriter& writer) {
                    writer.WriteU64(value.match.value);
                    writer.WriteBytes(value.ticket);
                    writer.WriteString8(value.host);
                    writer.WriteU16(value.tcpPort);
                    writer.WriteU16(value.udpPort);
                    writer.WriteU16(value.expiresInSeconds);
                });
            },
            [](const ErrorResponse& value) {
                if (!IsValidLobbyError(value.error))
                {
                    throw std::invalid_argument{"lobby error value is invalid"};
                }
                return EncodePayload(MessageType::ErrorResponse, value.requestId, [&](ByteWriter& writer) {
                    writer.WriteU16(static_cast<std::uint16_t>(value.error));
                });
            }},
        message);
}

MessageDecodeResult<ClientMessage> DecodeClientMessage(
    const MessageType type,
    const std::span<const std::byte> payload)
{
    switch (type)
    {
    case MessageType::ClientHello:
        return DecodeRequestOnly<ClientHello>(payload);
    case MessageType::ListRoomsRequest:
        return DecodeRequestOnly<ListRoomsRequest>(payload);
    case MessageType::CreateRoomRequest:
        return DecodeRequestOnly<CreateRoomRequest>(payload);
    case MessageType::JoinRoomRequest:
        return DecodeJoinRoom(payload);
    case MessageType::LeaveRoomRequest:
        return DecodeRequestOnly<LeaveRoomRequest>(payload);
    case MessageType::SetReadyRequest:
        return DecodeSetReady(payload);
    case MessageType::StartMatchRequest:
        return DecodeRequestOnly<StartMatchRequest>(payload);
    case MessageType::ServerWelcome:
    case MessageType::RoomListResponse:
    case MessageType::RoomSnapshot:
    case MessageType::MatchTicket:
    case MessageType::ErrorResponse:
    case MessageType::WorkerRegister:
    case MessageType::WorkerRegistered:
    case MessageType::ReserveMatch:
    case MessageType::ReserveMatchReady:
    case MessageType::ReserveMatchRejected:
    case MessageType::CancelMatchReservation:
    case MessageType::MatchReservationCancelled:
    case MessageType::MatchFinished:
    case MessageType::GameClientHello:
    case MessageType::GameServerWelcome:
    case MessageType::GameServerError:
    case MessageType::GameMatchResult:
        return Failure<ClientMessage>(DecodeError::InvalidValue);
    }
    return Failure<ClientMessage>(DecodeError::InvalidValue);
}

MessageDecodeResult<ServerMessage> DecodeServerMessage(
    const MessageType type,
    const std::span<const std::byte> payload)
{
    switch (type)
    {
    case MessageType::ServerWelcome:
        return DecodeWelcome(payload);
    case MessageType::RoomListResponse:
        return DecodeRoomList(payload);
    case MessageType::RoomSnapshot:
        return DecodeRoomSnapshot(payload);
    case MessageType::MatchTicket:
        return DecodeMatchTicket(payload);
    case MessageType::ErrorResponse:
        return DecodeErrorResponse(payload);
    case MessageType::ClientHello:
    case MessageType::ListRoomsRequest:
    case MessageType::CreateRoomRequest:
    case MessageType::JoinRoomRequest:
    case MessageType::LeaveRoomRequest:
    case MessageType::SetReadyRequest:
    case MessageType::StartMatchRequest:
    case MessageType::WorkerRegister:
    case MessageType::WorkerRegistered:
    case MessageType::ReserveMatch:
    case MessageType::ReserveMatchReady:
    case MessageType::ReserveMatchRejected:
    case MessageType::CancelMatchReservation:
    case MessageType::MatchReservationCancelled:
    case MessageType::MatchFinished:
    case MessageType::GameClientHello:
    case MessageType::GameServerWelcome:
    case MessageType::GameServerError:
    case MessageType::GameMatchResult:
        return Failure<ServerMessage>(DecodeError::InvalidValue);
    }
    return Failure<ServerMessage>(DecodeError::InvalidValue);
}
} // namespace dxa::protocol
