# Lobby Room Flow Implementation Plan

> For agentic workers: REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task by task. Steps use checkbox syntax for tracking. This milestone stays in the current local workflow and does not use subagents unless the user explicitly requests them.

Goal: Build a real Boost.Asio TCP lobby where a console client and 1 to 23 bot connections share one protocol and client transport to create, join, ready, start, and receive per-player match tickets.

Architecture: `dxa_protocol` owns strong IDs, messages, little-endian serialization, and the 64KiB frame contract. `dxa_lobby_core` owns connection, room, worker, and ticket state without socket dependencies. A single-threaded Asio adapter routes framed sessions into the service, while `dxa_lobby_client` is shared by the console and bot executables.

Tech Stack: C++20, CMake, vcpkg manifest, Boost.Asio, GoogleTest, spdlog, Windows BCrypt, Linux getrandom.

Spec: `docs/superpowers/specs/2026-08-24-lobby-room-flow-design.md`

## Global Constraints

- TCP frame header is 12 bytes and total frame size is at most 65,536 bytes.
- Payload size is at most 65,524 bytes and is validated before allocation.
- Protocol version is 1 and every integer uses explicit little-endian serialization.
- `PlayerId`, `RoomId`, `MatchId`, and `EntityId` are distinct value types.
- Request ID 0 is reserved for server push; client request IDs strictly increase per connection.
- Room capacity is 24, minimum start population is 2, and maximum room count is 1,024.
- Host is included in the all-ready condition.
- Host departure in Waiting transfers ownership to the earliest remaining join ordinal.
- Worker allocation failure restores Waiting and preserves ready values.
- Match tickets are 128-bit, unique per participant, one-use, and expire after 60 seconds.
- Production ticket bytes come from BCryptGenRandom on Windows and getrandom on Linux.
- Default server bind address is `127.0.0.1`; external bind requires an explicit option.
- Lobby state mutates only on one `boost::asio::io_context` thread in v1.
- Ticket bytes, passwords, personal information, and internal exception strings are never logged or sent.
- Existing DX11 client, renderer, simulation, and benchmark raw files remain unchanged.
- Korean noun-form Conventional Commit subjects are used. Every commit body records 이유, 핵심 변경, 검증.
- No benchmark or evidence directory is overwritten.

---

## File Map

### Protocol contracts

- `protocol/CMakeLists.txt`: `dxa_protocol` and `dxa_protocol_asio` targets, Boost.Asio dependency.
- `protocol/include/dxa/protocol/Ids.hpp`: strong ID types and numeric ordering.
- `protocol/include/dxa/protocol/LobbyTypes.hpp`: version, size limits, message, room, and error enums.
- `protocol/include/dxa/protocol/ByteCodec.hpp`: bounded little-endian reader and writer.
- `protocol/include/dxa/protocol/TcpFrame.hpp`: 12-byte header and encoded frame boundary.
- `protocol/include/dxa/protocol/LobbyMessages.hpp`: client and server message variants.
- `protocol/include/dxa/protocol/LobbyMessageCodec.hpp`: variant encode and decode results.
- `protocol/include/dxa/protocol/AsioFramedConnection.hpp`: async frame reader and ordered write queue.
- `protocol/src/ByteCodec.cpp`: integer, byte array, string, and cursor implementation.
- `protocol/src/TcpFrame.cpp`: magic, version, type, and payload validation.
- `protocol/src/LobbyMessageCodec.cpp`: every lobby payload encoder and decoder.
- `protocol/src/AsioFramedConnection.cpp`: async header, payload, queue, and close lifecycle.

### Lobby server and domain

- `apps/lobby_server/CMakeLists.txt`: core, server adapter, executable, and bcrypt link.
- `apps/lobby_server/include/dxa/lobby/ConnectionId.hpp`: process-local connection routing key.
- `apps/lobby_server/include/dxa/lobby/Room.hpp`: participant state and room transitions.
- `apps/lobby_server/include/dxa/lobby/MatchTicketRegistry.hpp`: ticket source, issue, consume, and expiry.
- `apps/lobby_server/include/dxa/lobby/GameWorkerAllocator.hpp`: allocator interface and static implementation.
- `apps/lobby_server/include/dxa/lobby/LobbyService.hpp`: session and room command aggregate.
- `apps/lobby_server/include/dxa/lobby/LobbyTcpServer.hpp`: acceptor and TCP session routing.
- `apps/lobby_server/include/dxa/lobby/LobbyServerOptions.hpp`: bind, port, and static worker options.
- `apps/lobby_server/src/Room.cpp`: pure room rules.
- `apps/lobby_server/src/MatchTicketRegistry.cpp`: collision-bounded issue and consume.
- `apps/lobby_server/src/SecureTicketSource.cpp`: BCrypt and getrandom implementations.
- `apps/lobby_server/src/GameWorkerAllocator.cpp`: configured endpoint validation.
- `apps/lobby_server/src/LobbyService.cpp`: request order, room indexes, broadcasts, and start flow.
- `apps/lobby_server/src/LobbyTcpServer.cpp`: network session to service adapter.
- `apps/lobby_server/src/LobbyServerOptions.cpp`: CLI option parser.
- `apps/lobby_server/src/main.cpp`: signal handling and server run loop.

### Shared client and executable clients

- `apps/lobby_client/CMakeLists.txt`: reusable `dxa_lobby_client` target.
- `apps/lobby_client/include/dxa/lobby_client/LobbyClient.hpp`: async connect, typed requests, callbacks.
- `apps/lobby_client/src/LobbyClient.cpp`: request sequence and server message decode.
- `apps/lobby_cli/CMakeLists.txt`: human console executable.
- `apps/lobby_cli/include/dxa/lobby_cli/LobbyCliCommand.hpp`: command parser contract.
- `apps/lobby_cli/include/dxa/lobby_cli/LobbyCliOutput.hpp`: server message formatter with ticket redaction.
- `apps/lobby_cli/src/LobbyCliCommand.cpp`: list, create, join, leave, ready, start, quit parsing.
- `apps/lobby_cli/src/LobbyCliOutput.cpp`: human-readable welcome, room, error, and redacted ticket output.
- `apps/lobby_cli/src/main.cpp`: input thread, callback output, and ticket redaction.
- `apps/bot_client/CMakeLists.txt`: headless bot executable.
- `apps/bot_client/include/dxa/bot_client/BotClientOptions.hpp`: host, port, room, and count parser.
- `apps/bot_client/src/BotClientOptions.cpp`: option validation.
- `apps/bot_client/src/main.cpp`: 1 to 23 shared-client bot state machines.

### Tests and records

- `tests/protocol_ids_test.cpp`
- `tests/protocol_byte_codec_test.cpp`
- `tests/protocol_tcp_frame_test.cpp`
- `tests/protocol_lobby_message_codec_test.cpp`
- `tests/protocol_asio_framed_connection_test.cpp`
- `tests/lobby_room_test.cpp`
- `tests/lobby_ticket_test.cpp`
- `tests/lobby_service_test.cpp`
- `tests/lobby_tcp_integration_test.cpp`
- `tests/support/lobby_network_fixture.hpp`
- `tests/lobby_server_options_test.cpp`
- `tests/lobby_cli_command_test.cpp`
- `tests/lobby_cli_output_test.cpp`
- `tests/bot_client_options_test.cpp`
- `docs/adr/0006-lobby-domain-and-tcp-adapter.md`
- `docs/devlog/2026-08-24-lobby-room-flow.md`
- `README.md`
- `docs/PROJECT_PLAN.md`

---

### Task 1: Strong IDs and locked lobby enums

Files:

- Create: `protocol/CMakeLists.txt`
- Create: `protocol/include/dxa/protocol/Ids.hpp`
- Create: `protocol/include/dxa/protocol/LobbyTypes.hpp`
- Create: `tests/protocol_ids_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Produces: `PlayerId`, `RoomId`, `MatchId`, `EntityId` with defaulted three-way comparison.
- Produces: `ProtocolVersion`, frame limits, room limits, ticket limits, and pending write limit.
- Produces: `RoomState`, `MessageType`, `LobbyError` with locked wire values.

- [ ] Step 1: Add the failing contract test

Add `protocol_ids_test.cpp` to `dxa_tests` before creating the protocol include directory.

```cpp
#include <dxa/protocol/Ids.hpp>
#include <dxa/protocol/LobbyTypes.hpp>

#include <gtest/gtest.h>

#include <type_traits>

TEST(ProtocolIds, KeepsDomainIdsDistinct)
{
    static_assert(!std::is_same_v<dxa::protocol::PlayerId, dxa::protocol::RoomId>);
    static_assert(!std::is_same_v<dxa::protocol::RoomId, dxa::protocol::MatchId>);
    EXPECT_LT(dxa::protocol::PlayerId{1U}, dxa::protocol::PlayerId{2U});
}

TEST(LobbyTypes, LocksWireConstants)
{
    EXPECT_EQ(1U, dxa::protocol::ProtocolVersion);
    EXPECT_EQ(12U, dxa::protocol::TcpFrameHeaderBytes);
    EXPECT_EQ(65536U, dxa::protocol::MaxTcpFrameBytes);
    EXPECT_EQ(65524U, dxa::protocol::MaxTcpPayloadBytes);
    EXPECT_EQ(std::size_t{24}, dxa::protocol::RoomCapacity);
    EXPECT_EQ(std::size_t{1024}, dxa::protocol::MaximumRooms);
    EXPECT_EQ(std::size_t{16}, dxa::protocol::MatchTicketBytes);
    EXPECT_EQ(std::size_t{60}, dxa::protocol::MatchTicketLifetimeSeconds);
    EXPECT_EQ(std::size_t{262144}, dxa::protocol::MaxPendingWriteBytes);
    EXPECT_EQ(1U, static_cast<unsigned>(dxa::protocol::MessageType::ClientHello));
    EXPECT_EQ(12U, static_cast<unsigned>(dxa::protocol::MessageType::ErrorResponse));
}
```

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: FAIL because `dxa/protocol/Ids.hpp` does not exist.

- [ ] Step 3: Add the protocol target and types

Put `add_subdirectory(protocol)` before `simulation` in the root CMake file. Define `dxa_protocol` as an `INTERFACE` library in this header-only task and expose `protocol/include`. Task 2 converts it to a static library when `ByteCodec.cpp` becomes the first source file.

```cpp
struct PlayerId
{
    std::uint32_t value = 0;
    [[nodiscard]] auto operator<=>(const PlayerId&) const = default;
};

struct RoomId
{
    std::uint32_t value = 0;
    [[nodiscard]] auto operator<=>(const RoomId&) const = default;
};

struct MatchId
{
    std::uint64_t value = 0;
    [[nodiscard]] auto operator<=>(const MatchId&) const = default;
};

struct EntityId
{
    std::uint32_t value = 0;
    [[nodiscard]] auto operator<=>(const EntityId&) const = default;
};
```

Define every enum with a fixed underlying type. `MessageType` uses values 1 through 12 in the spec. `RoomState` uses Waiting 1, Starting 2, InMatch 3. `LobbyError` uses consecutive values beginning at 1 in the spec order. Define `RoomCapacity=24`, `MaximumRooms=1024`, `MatchTicketBytes=16`, `MatchTicketLifetimeSeconds=60`, and `MaxPendingWriteBytes=262144` as `inline constexpr std::size_t` values in `LobbyTypes.hpp`.

- [ ] Step 4: Run GREEN

Run: `./scripts/build.ps1`

Run: `./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=ProtocolIds.*:LobbyTypes.*`

Expected: 2 tests pass.

- [ ] Step 5: Commit

```powershell
git add -- CMakeLists.txt protocol/CMakeLists.txt protocol/include/dxa/protocol/Ids.hpp protocol/include/dxa/protocol/LobbyTypes.hpp tests/CMakeLists.txt tests/protocol_ids_test.cpp
git commit -m "feat(protocol): 로비 ID와 wire enum 추가" `
  -m "이유: room과 match ID 혼용을 막고 이후 codec이 사용할 숫자 계약을 먼저 고정해야 했다." `
  -m "핵심 변경: strong ID 네 종류, protocol 및 frame 상수, room과 message 및 error enum을 추가했다." `
  -m "검증: 헤더 부재 RED를 확인한 뒤 ProtocolIds와 LobbyTypes 테스트를 통과했다."
```

---

### Task 2: Bounded little-endian byte codec

Files:

- Create: `protocol/include/dxa/protocol/ByteCodec.hpp`
- Create: `protocol/src/ByteCodec.cpp`
- Create: `tests/protocol_byte_codec_test.cpp`
- Modify: `protocol/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Produces: `ByteWriter::WriteU8`, `WriteU16`, `WriteU32`, `WriteU64`, `WriteBytes`, `WriteString8`, `Finish`.
- Produces: `ByteReader::ReadU8`, `ReadU16`, `ReadU32`, `ReadU64`, `ReadBytes`, `ReadString8`, `Empty`, `Remaining`.
- Produces: `DecodeError { None, Truncated, InvalidValue, CountLimit, TrailingBytes }`.

- [ ] Step 1: Write exact-byte and boundary tests

```cpp
TEST(ByteCodec, WritesAndReadsLittleEndianValues)
{
    dxa::protocol::ByteWriter writer;
    writer.WriteU16(0x1234U);
    writer.WriteU32(0x89ABCDEFU);
    writer.WriteU64(0x0102030405060708ULL);
    const std::vector<std::byte> bytes = std::move(writer).Finish();

    EXPECT_EQ(std::byte{0x34}, bytes[0]);
    EXPECT_EQ(std::byte{0x12}, bytes[1]);
    EXPECT_EQ(std::byte{0xEF}, bytes[2]);

    dxa::protocol::ByteReader reader{bytes};
    const auto u16 = reader.ReadU16();
    const auto u32 = reader.ReadU32();
    const auto u64 = reader.ReadU64();
    ASSERT_TRUE(u16.has_value());
    ASSERT_TRUE(u32.has_value());
    ASSERT_TRUE(u64.has_value());
    EXPECT_EQ(0x1234U, *u16);
    EXPECT_EQ(0x89ABCDEFU, *u32);
    EXPECT_EQ(0x0102030405060708ULL, *u64);
    EXPECT_TRUE(reader.Empty());
}

TEST(ByteCodec, RejectsTruncatedAndOversizedString)
{
    constexpr std::array oneByte{std::byte{0x01}};
    dxa::protocol::ByteReader truncated{oneByte};
    EXPECT_FALSE(truncated.ReadU32().has_value());
    EXPECT_EQ(dxa::protocol::DecodeError::Truncated, truncated.Error());

    dxa::protocol::ByteWriter writer;
    EXPECT_THROW(writer.WriteString8(std::string(256U, 'x')), std::length_error);
}
```

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: FAIL because `ByteCodec.hpp` does not exist.

- [ ] Step 3: Implement a cursor that never reads past its span

Convert `dxa_protocol` from `INTERFACE` to `STATIC` with `src/ByteCodec.cpp` and keep the same public include directory. The reader returns `std::optional<T>` and latches the first `DecodeError`. Every read after an error returns empty. `ReadString8` reads one byte of length and rejects a caller maximum smaller than the encoded count. `Finish` moves out the writer vector.

```cpp
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
};
```

- [ ] Step 4: Run GREEN and the protocol subset

Run: `./scripts/build.ps1`

Run: `./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=ByteCodec.*:ProtocolIds.*:LobbyTypes.*`

Expected: all selected tests pass without warnings.

- [ ] Step 5: Commit

```powershell
git add -- protocol/CMakeLists.txt protocol/include/dxa/protocol/ByteCodec.hpp protocol/src/ByteCodec.cpp tests/CMakeLists.txt tests/protocol_byte_codec_test.cpp
git commit -m "feat(protocol): bounded little-endian codec 추가" `
  -m "이유: 구조체 memory를 보내지 않고 untrusted payload를 allocation과 cursor 경계 안에서 읽어야 했다." `
  -m "핵심 변경: 정수, byte, 길이 제한 문자열 writer와 첫 오류를 유지하는 reader를 구현했다." `
  -m "검증: ByteCodec 헤더 부재 RED 뒤 exact byte, truncated input, 255바이트 문자열 경계를 통과했다."
```

---

### Task 3: 64KiB TCP frame boundary

Files:

- Create: `protocol/include/dxa/protocol/TcpFrame.hpp`
- Create: `protocol/src/TcpFrame.cpp`
- Create: `tests/protocol_tcp_frame_test.cpp`
- Modify: `protocol/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: `MessageType`, protocol constants, `ByteReader`, `ByteWriter`.
- Produces: `TcpFrameHeader`, `FrameHeaderDecodeResult`, `EncodedMessage`.
- Produces: `EncodeTcpFrame`, `DecodeTcpFrameHeader`.

- [ ] Step 1: Write frame header RED tests

```cpp
TEST(TcpFrame, EncodesTwelveByteHeaderAndPayload)
{
    const dxa::protocol::EncodedMessage message{
        dxa::protocol::MessageType::ClientHello,
        {std::byte{0x11}, std::byte{0x22}}};
    const auto frame = dxa::protocol::EncodeTcpFrame(message);

    ASSERT_EQ(14U, frame.size());
    EXPECT_EQ(std::byte{0x44}, frame[0]);
    EXPECT_EQ(std::byte{0x58}, frame[1]);
    EXPECT_EQ(std::byte{0x41}, frame[2]);
    EXPECT_EQ(std::byte{0x31}, frame[3]);
    EXPECT_EQ(std::byte{0x02}, frame[8]);
}

TEST(TcpFrame, RejectsOversizedPayloadBeforeAllocation)
{
    const std::array<std::byte, dxa::protocol::TcpFrameHeaderBytes> header{
        std::byte{0x44}, std::byte{0x58}, std::byte{0x41}, std::byte{0x31},
        std::byte{0x01}, std::byte{0x00}, std::byte{0x01}, std::byte{0x00},
        std::byte{0xF5}, std::byte{0xFF}, std::byte{0x00}, std::byte{0x00}};
    const auto decoded = dxa::protocol::DecodeTcpFrameHeader(header);
    EXPECT_EQ(dxa::protocol::FrameHeaderError::FrameTooLarge, decoded.error);
    EXPECT_FALSE(decoded.header.has_value());
}
```

Also cover wrong magic, version, unknown type, 65,536-byte inclusive boundary, and a header span not exactly 12 bytes.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: FAIL because `TcpFrame.hpp` does not exist.

- [ ] Step 3: Implement header encoding and fail-closed decoding

```cpp
struct TcpFrameHeader
{
    std::uint16_t version = ProtocolVersion;
    MessageType type = MessageType::ClientHello;
    std::uint32_t payloadBytes = 0;
};

struct EncodedMessage
{
    MessageType type = MessageType::ClientHello;
    std::vector<std::byte> payload;
};

enum class FrameHeaderError
{
    None,
    InvalidHeaderSize,
    BadMagic,
    UnsupportedVersion,
    UnknownMessageType,
    FrameTooLarge
};

struct FrameHeaderDecodeResult
{
    std::optional<TcpFrameHeader> header;
    FrameHeaderError error = FrameHeaderError::None;
};
```

Compare the first four bytes to ASCII `DXA1`. Validate version, type range, and payload length before returning a header. `EncodeTcpFrame` throws `std::length_error` when payload exceeds 65,524 bytes.

- [ ] Step 4: Run GREEN

Run: `./scripts/build.ps1`

Run: `./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=TcpFrame.*:ByteCodec.*`

Expected: frame and codec tests pass.

- [ ] Step 5: Commit

```powershell
git add -- protocol/CMakeLists.txt protocol/include/dxa/protocol/TcpFrame.hpp protocol/src/TcpFrame.cpp tests/CMakeLists.txt tests/protocol_tcp_frame_test.cpp
git commit -m "feat(protocol): 64KiB TCP frame 경계 추가" `
  -m "이유: payload allocation 전에 magic, version, type과 길이를 검증하는 고정 header가 필요했다." `
  -m "핵심 변경: 12바이트 frame header와 65,536바이트 전체 크기 제한, fail-closed decode 결과를 구현했다." `
  -m "검증: TcpFrame 헤더 부재 RED 뒤 최대 경계, 과대 길이, magic과 version 및 unknown type 검사를 통과했다."
```

---

### Task 4: Lobby message schema and serialization

Files:

- Create: `protocol/include/dxa/protocol/LobbyMessages.hpp`
- Create: `protocol/include/dxa/protocol/LobbyMessageCodec.hpp`
- Create: `protocol/src/LobbyMessageCodec.cpp`
- Create: `tests/protocol_lobby_message_codec_test.cpp`
- Modify: `protocol/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Produces every message struct and `ClientMessage`, `ServerMessage` variants.
- Produces `RequestId(const ClientMessage&)`, `RequestId(const ServerMessage&)`.
- Produces `EncodeClientMessage`, `EncodeServerMessage`, `DecodeClientMessage`, `DecodeServerMessage`.

- [ ] Step 1: Write round-trip and semantic boundary tests

```cpp
TEST(LobbyMessageCodec, RoundTripsJoinRoomWithExactWireOrder)
{
    const dxa::protocol::ClientMessage source =
        dxa::protocol::JoinRoomRequest{7U, dxa::protocol::RoomId{42U}};
    const auto encoded = dxa::protocol::EncodeClientMessage(source);

    EXPECT_EQ(dxa::protocol::MessageType::JoinRoomRequest, encoded.type);
    ASSERT_EQ(8U, encoded.payload.size());
    EXPECT_EQ(std::byte{0x07}, encoded.payload[0]);
    EXPECT_EQ(std::byte{0x2A}, encoded.payload[4]);

    const auto decoded = dxa::protocol::DecodeClientMessage(
        encoded.type,
        encoded.payload);
    ASSERT_TRUE(decoded.message.has_value());
    EXPECT_EQ(source, *decoded.message);
}

TEST(LobbyMessageCodec, RejectsMemberCountAboveTwentyFour)
{
    dxa::protocol::ByteWriter writer;
    writer.WriteU32(1U);
    writer.WriteU32(2U);
    writer.WriteU8(static_cast<std::uint8_t>(dxa::protocol::RoomState::Waiting));
    writer.WriteU32(3U);
    writer.WriteU8(25U);
    const auto payload = std::move(writer).Finish();
    const auto decoded = dxa::protocol::DecodeServerMessage(
        dxa::protocol::MessageType::RoomSnapshot,
        payload);
    EXPECT_EQ(dxa::protocol::DecodeError::CountLimit, decoded.error);
}
```

Add a table-driven round-trip case for all 12 message types. Add tests for bool values other than 0 or 1, room count above 1,024, host length 255 and 256, invalid enum values, zero client request ID, and trailing bytes.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: FAIL because `LobbyMessages.hpp` and `LobbyMessageCodec.hpp` do not exist.

- [ ] Step 3: Define the schema exactly once

```cpp
struct ClientHello { std::uint32_t requestId = 0; };
struct ListRoomsRequest { std::uint32_t requestId = 0; };
struct CreateRoomRequest { std::uint32_t requestId = 0; };
struct JoinRoomRequest { std::uint32_t requestId = 0; RoomId room; };
struct LeaveRoomRequest { std::uint32_t requestId = 0; };
struct SetReadyRequest { std::uint32_t requestId = 0; bool ready = false; };
struct StartMatchRequest { std::uint32_t requestId = 0; };

struct RoomSummary { RoomId room; std::uint8_t players = 0; std::uint8_t capacity = 24; };
struct RoomMemberView { PlayerId player; bool ready = false; };
struct ServerWelcome { std::uint32_t requestId = 0; PlayerId player; };
struct RoomListResponse { std::uint32_t requestId = 0; std::vector<RoomSummary> rooms; };
struct RoomSnapshot { std::uint32_t requestId = 0; RoomId room; RoomState state; PlayerId host; std::vector<RoomMemberView> members; };
struct MatchTicket { std::uint32_t requestId = 0; MatchId match; std::array<std::byte, 16> ticket; std::string host; std::uint16_t tcpPort = 0; std::uint16_t udpPort = 0; std::uint16_t expiresInSeconds = 60; };
struct ErrorResponse { std::uint32_t requestId = 0; LobbyError error = LobbyError::InternalError; };

using ClientMessage = std::variant<
    ClientHello,
    ListRoomsRequest,
    CreateRoomRequest,
    JoinRoomRequest,
    LeaveRoomRequest,
    SetReadyRequest,
    StartMatchRequest>;

using ServerMessage = std::variant<
    ServerWelcome,
    RoomListResponse,
    RoomSnapshot,
    MatchTicket,
    ErrorResponse>;

template <typename MessageVariant>
struct MessageDecodeResult
{
    std::optional<MessageVariant> message;
    DecodeError error = DecodeError::None;
};

[[nodiscard]] EncodedMessage EncodeClientMessage(const ClientMessage& message);
[[nodiscard]] EncodedMessage EncodeServerMessage(const ServerMessage& message);
[[nodiscard]] std::uint32_t RequestId(const ClientMessage& message) noexcept;
[[nodiscard]] std::uint32_t RequestId(const ServerMessage& message) noexcept;
[[nodiscard]] MessageDecodeResult<ClientMessage> DecodeClientMessage(
    MessageType type,
    std::span<const std::byte> payload);
[[nodiscard]] MessageDecodeResult<ServerMessage> DecodeServerMessage(
    MessageType type,
    std::span<const std::byte> payload);
```

Give every message and view struct a defaulted equality operator. Sort room summaries by RoomId and member views by PlayerId before encoding. Decoder constructors validate every count and enum and require `ByteReader::Empty()` at the end.

- [ ] Step 4: Run GREEN and full protocol tests

Run: `./scripts/build.ps1`

Run: `./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=LobbyMessageCodec.*:TcpFrame.*:ByteCodec.*:ProtocolIds.*:LobbyTypes.*`

Expected: all protocol tests pass.

- [ ] Step 5: Commit

```powershell
git add -- protocol/CMakeLists.txt protocol/include/dxa/protocol/LobbyMessages.hpp protocol/include/dxa/protocol/LobbyMessageCodec.hpp protocol/src/LobbyMessageCodec.cpp tests/CMakeLists.txt tests/protocol_lobby_message_codec_test.cpp
git commit -m "feat(protocol): 로비 message codec 추가" `
  -m "이유: client와 server가 같은 packet schema를 사용하고 vector 및 string count를 payload 경계에서 검증해야 했다." `
  -m "핵심 변경: 12개 message variant, request ID, room view, ticket과 error payload의 encode 및 decode를 구현했다." `
  -m "검증: codec 부재 RED 뒤 전체 message round-trip, exact byte, count와 enum 및 trailing byte 거부를 통과했다."
```

---

### Task 5: Pure Room state machine

Files:

- Create: `apps/lobby_server/CMakeLists.txt`
- Create: `apps/lobby_server/include/dxa/lobby/ConnectionId.hpp`
- Create: `apps/lobby_server/include/dxa/lobby/Room.hpp`
- Create: `apps/lobby_server/src/Room.cpp`
- Create: `tests/lobby_room_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: protocol IDs, `RoomState`, `LobbyError`, `RoomSnapshot`.
- Produces: `ConnectionId` and `Room` transition API.

- [ ] Step 1: Write room transition RED tests

```cpp
TEST(LobbyRoom, TransfersHostToEarliestRemainingJoin)
{
    dxa::lobby::Room room = dxa::lobby::Room::Create(
        dxa::protocol::RoomId{1U},
        dxa::protocol::PlayerId{10U},
        100U);
    ASSERT_FALSE(room.Join(dxa::protocol::PlayerId{30U}, 102U).has_value());
    ASSERT_FALSE(room.Join(dxa::protocol::PlayerId{20U}, 101U).has_value());

    ASSERT_FALSE(room.Leave(dxa::protocol::PlayerId{10U}).has_value());

    EXPECT_EQ(dxa::protocol::PlayerId{20U}, room.Host());
}

TEST(LobbyRoom, AcceptsTwentyFourthAndRejectsTwentyFifthPlayer)
{
    dxa::lobby::Room room = RoomWithPlayers(23U);
    EXPECT_FALSE(room.Join(dxa::protocol::PlayerId{24U}, 24U).has_value());
    EXPECT_EQ(
        dxa::protocol::LobbyError::RoomFull,
        room.Join(dxa::protocol::PlayerId{25U}, 25U));
}
```

Define the helper in the same test file so it has no hidden fixture behavior.

```cpp
[[nodiscard]] dxa::lobby::Room RoomWithPlayers(const std::uint32_t count)
{
    dxa::lobby::Room room = dxa::lobby::Room::Create(
        dxa::protocol::RoomId{1U},
        dxa::protocol::PlayerId{1U},
        1U);
    for (std::uint32_t value = 2U; value <= count; ++value)
    {
        const auto error = room.Join(
            dxa::protocol::PlayerId{value},
            value);
        if (error.has_value())
        {
            throw std::logic_error{"test room setup failed"};
        }
    }
    return room;
}
```

Add tests for host ready inclusion, minimum two players, non-host start, not-all-ready start, state-specific mutation rejection, empty room, and PlayerId-sorted snapshot.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: FAIL because `dxa/lobby/Room.hpp` does not exist.

- [ ] Step 3: Implement room rules without socket code

```cpp
struct ConnectionId
{
    std::uint64_t value = 0;
    [[nodiscard]] auto operator<=>(const ConnectionId&) const = default;
};

struct RoomMember
{
    dxa::protocol::PlayerId player;
    bool ready = false;
    std::uint64_t joinOrdinal = 0;
};

class Room
{
public:
    [[nodiscard]] static Room Create(RoomId room, PlayerId host, std::uint64_t joinOrdinal);
    [[nodiscard]] std::optional<LobbyError> Join(PlayerId player, std::uint64_t joinOrdinal);
    [[nodiscard]] std::optional<LobbyError> Leave(PlayerId player);
    [[nodiscard]] std::optional<LobbyError> SetReady(PlayerId player, bool ready);
    [[nodiscard]] std::optional<LobbyError> ValidateStart(PlayerId requester) const;
    void BeginStarting();
    void ReturnToWaiting();
    void MarkInMatch();
    [[nodiscard]] bool Contains(PlayerId player) const noexcept;
    [[nodiscard]] bool Empty() const noexcept;
    [[nodiscard]] PlayerId Host() const;
    [[nodiscard]] std::vector<PlayerId> Players() const;
    [[nodiscard]] RoomSnapshot Snapshot(std::uint32_t requestId) const;
};
```

Store members in `std::map<PlayerId, RoomMember>`. Select a replacement host with `std::min_element` over join ordinal, then PlayerId for the impossible equal-ordinal tie. Never expose join ordinal in snapshots.

- [ ] Step 4: Run GREEN

Run: `./scripts/build.ps1`

Run: `./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=LobbyRoom.*`

Expected: all room tests pass.

- [ ] Step 5: Commit

```powershell
git add -- CMakeLists.txt apps/lobby_server/CMakeLists.txt apps/lobby_server/include/dxa/lobby/ConnectionId.hpp apps/lobby_server/include/dxa/lobby/Room.hpp apps/lobby_server/src/Room.cpp tests/CMakeLists.txt tests/lobby_room_test.cpp
git commit -m "feat(lobby): 24인 room state machine 추가" `
  -m "이유: TCP lifetime과 분리된 상태에서 정원, 준비, 시작과 방장 승계를 결정적으로 검증해야 했다." `
  -m "핵심 변경: process-local ConnectionId와 Waiting, Starting, InMatch 전이를 가진 pure Room을 구현했다." `
  -m "검증: Room 헤더 부재 RED 뒤 24번째 입장, 25번째 거부, host 승계와 start 조건 테스트를 통과했다."
```

---

### Task 6: Secure one-use match tickets

Files:

- Create: `apps/lobby_server/include/dxa/lobby/MatchTicketRegistry.hpp`
- Create: `apps/lobby_server/src/MatchTicketRegistry.cpp`
- Create: `apps/lobby_server/src/SecureTicketSource.cpp`
- Create: `tests/lobby_ticket_test.cpp`
- Modify: `apps/lobby_server/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: `MatchId`, `PlayerId`, 16-byte protocol ticket.
- Produces: `ITicketSource`, `SecureTicketSource`, `MatchTicketRegistry`, and `MatchTicketRegistry::Revoke`.

- [ ] Step 1: Write deterministic issue, expiry, and consume RED tests

```cpp
TEST(MatchTicketRegistry, IssuesDistinctTicketsAndConsumesOnce)
{
    SequenceTicketSource source{{Ticket(1U), Ticket(2U)}};
    dxa::lobby::MatchTicketRegistry registry{source};
    const auto now = std::chrono::steady_clock::time_point{};

    const auto first = registry.Issue(MatchId{7U}, PlayerId{1U}, now);
    const auto second = registry.Issue(MatchId{7U}, PlayerId{2U}, now);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_NE(*first, *second);
    EXPECT_EQ(TicketConsumeResult::Accepted, registry.Consume(*first, MatchId{7U}, PlayerId{1U}, now));
    EXPECT_EQ(TicketConsumeResult::NotFound, registry.Consume(*first, MatchId{7U}, PlayerId{1U}, now));
}

TEST(MatchTicketRegistry, RejectsExpiredTicket)
{
    SequenceTicketSource source{{Ticket(9U)}};
    dxa::lobby::MatchTicketRegistry registry{source};
    const auto issued = registry.Issue(MatchId{1U}, PlayerId{1U}, Time(0));
    ASSERT_TRUE(issued.has_value());
    EXPECT_EQ(
        TicketConsumeResult::Expired,
        registry.Consume(*issued, MatchId{1U}, PlayerId{1U}, Time(61)));
}
```

Define deterministic helpers in `lobby_ticket_test.cpp`.

```cpp
[[nodiscard]] MatchTicketValue Ticket(const std::uint8_t seed)
{
    MatchTicketValue value{};
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        value[index] = std::byte{static_cast<std::uint8_t>(seed + index)};
    }
    return value;
}

[[nodiscard]] auto Time(const std::int64_t seconds)
{
    return std::chrono::steady_clock::time_point{std::chrono::seconds{seconds}};
}

class SequenceTicketSource final : public ITicketSource
{
public:
    explicit SequenceTicketSource(
        std::initializer_list<std::optional<MatchTicketValue>> sequence);
    [[nodiscard]] bool Fill(std::span<std::byte, 16> output) noexcept override;

private:
    std::deque<std::optional<MatchTicketValue>> sequence_;
};
```

`Fill` pops one entry, copies all 16 bytes when it has a value, and returns false for an empty optional or exhausted sequence. Treat `now >= expiresAt` as expired, including exactly 60 seconds.

Add collision retry exhaustion after eight identical values, source failure, mismatch that does not consume, and expiry at exactly 60 seconds. Add a revoke test that issues two tickets, revokes both values, and expects both later consume attempts to return `NotFound`.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: FAIL because `MatchTicketRegistry.hpp` does not exist.

- [ ] Step 3: Implement registry and platform entropy

```cpp
using MatchTicketValue = std::array<std::byte, 16>;

enum class TicketConsumeResult
{
    Accepted,
    NotFound,
    Expired,
    Mismatch
};

class ITicketSource
{
public:
    virtual ~ITicketSource() = default;
    [[nodiscard]] virtual bool Fill(std::span<std::byte, 16> output) noexcept = 0;
};

class MatchTicketRegistry
{
public:
    explicit MatchTicketRegistry(ITicketSource& source) noexcept;
    [[nodiscard]] std::optional<MatchTicketValue> Issue(
        MatchId match,
        PlayerId player,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] TicketConsumeResult Consume(
        const MatchTicketValue& ticket,
        MatchId match,
        PlayerId player,
        std::chrono::steady_clock::time_point now);
    void Revoke(std::span<const MatchTicketValue> tickets) noexcept;
    void PurgeExpired(std::chrono::steady_clock::time_point now);
};
```

On Windows call `BCryptGenRandom(nullptr, bytes, 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG)` and link `bcrypt`. On Linux call `getrandom` in a loop until 16 bytes are filled, retry only on `EINTR`, and return false on all other errors. Do not add a `std::random_device` fallback.

- [ ] Step 4: Run GREEN

Run: `./scripts/build.ps1`

Run: `./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=MatchTicketRegistry.*`

Expected: ticket tests pass; no test prints ticket bytes.

- [ ] Step 5: Commit

```powershell
git add -- apps/lobby_server/CMakeLists.txt apps/lobby_server/include/dxa/lobby/MatchTicketRegistry.hpp apps/lobby_server/src/MatchTicketRegistry.cpp apps/lobby_server/src/SecureTicketSource.cpp tests/CMakeLists.txt tests/lobby_ticket_test.cpp
git commit -m "feat(lobby): 128비트 일회용 ticket 추가" `
  -m "이유: 참가자별 match 입장을 60초와 한 번의 consume으로 제한하고 보안 난수 근거를 플랫폼별로 명시해야 했다." `
  -m "핵심 변경: BCrypt와 getrandom source, collision 제한, 만료와 mismatch를 가진 ticket registry를 구현했다." `
  -m "검증: registry 헤더 부재 RED 뒤 distinct issue, 60초 만료, mismatch와 재사용 및 entropy failure 테스트를 통과했다."
```

---

### Task 7: LobbyService handshake and room lifecycle

Files:

- Create: `apps/lobby_server/include/dxa/lobby/GameWorkerAllocator.hpp`
- Create: `apps/lobby_server/include/dxa/lobby/LobbyService.hpp`
- Create: `apps/lobby_server/src/GameWorkerAllocator.cpp`
- Create: `apps/lobby_server/src/LobbyService.cpp`
- Create: `tests/lobby_service_test.cpp`
- Modify: `apps/lobby_server/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: protocol variants, Room, MatchTicketRegistry.
- Produces: worker endpoint types, `IGameWorkerAllocator`, `StaticGameWorkerAllocator`, `UnavailableGameWorkerAllocator`.
- Produces: `LobbyAuditEvent`, `LobbyServiceResult`, `LobbyService::OpenConnection`, `Handle`, `Disconnect`, `Tickets`.

- [ ] Step 1: Write handshake, request order, and room lifecycle RED tests

```cpp
TEST(LobbyService, RequiresHelloAndStrictlyIncreasingRequestIds)
{
    LobbyFixture fixture;
    const auto connection = fixture.service.OpenConnection();
    ASSERT_TRUE(connection.has_value());

    auto output = fixture.service.Handle(
        *connection,
        ClientMessage{ListRoomsRequest{1U}},
        Time(0));
    EXPECT_EQ(LobbyError::NotWelcomed, OnlyError(output).error);

    output = fixture.service.Handle(
        *connection,
        ClientMessage{ClientHello{2U}},
        Time(0));
    EXPECT_EQ(PlayerId{1U}, OnlyWelcome(output).player);

    output = fixture.service.Handle(
        *connection,
        ClientMessage{CreateRoomRequest{2U}},
        Time(0));
    EXPECT_EQ(LobbyError::RequestOutOfOrder, OnlyError(output).error);
    EXPECT_EQ(0U, fixture.service.RoomCount());
}

TEST(LobbyService, DisconnectTransfersHostAndDeletesEmptyRoom)
{
    LobbyFixture fixture;
    const auto host = WelcomedConnection(fixture, 1U);
    const auto next = WelcomedConnection(fixture, 2U);
    const RoomId room = CreateRoom(fixture, host, 2U);
    JoinRoom(fixture, next, 2U, room);

    const auto output = fixture.service.Disconnect(host);
    EXPECT_EQ(PlayerId{2U}, OnlySnapshot(output).host);
    EXPECT_TRUE(fixture.service.HasRoom(room));

    fixture.service.Disconnect(next);
    EXPECT_FALSE(fixture.service.HasRoom(room));
}
```

Add create, list sorted by RoomId, already-in-room, leave, ready broadcast request IDs, 1,024 room limit, and ID exhaustion tests through an injectable initial counter config.

Define these local fixture helpers in `lobby_service_test.cpp`:

```cpp
struct LobbyFixture
{
    SequenceTicketSource ticketSource;
    MatchTicketRegistry tickets;
    UnavailableGameWorkerAllocator allocator;
    LobbyService service;
};

[[nodiscard]] ConnectionId WelcomedConnection(
    LobbyFixture& fixture,
    std::uint32_t helloRequestId);
[[nodiscard]] RoomId CreateRoom(
    LobbyFixture& fixture,
    ConnectionId host,
    std::uint32_t requestId);
void JoinRoom(
    LobbyFixture& fixture,
    ConnectionId guest,
    std::uint32_t requestId,
    RoomId room);
[[nodiscard]] const ErrorResponse& OnlyError(
    const LobbyServiceResult& result);
[[nodiscard]] const ServerWelcome& OnlyWelcome(
    const LobbyServiceResult& result);
[[nodiscard]] const RoomSnapshot& OnlySnapshot(
    const LobbyServiceResult& result);
```

Each `Only` helper asserts one matching variant and throws `std::logic_error` on malformed fixture output. It must not repair or filter production output silently.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: FAIL because `LobbyService.hpp` does not exist.

- [ ] Step 3: Implement service connection and room indexes

```cpp
struct GameEndpoint
{
    std::string host;
    std::uint16_t tcpPort = 0;
    std::uint16_t udpPort = 0;
};

struct WorkerAllocationResult
{
    std::optional<GameEndpoint> endpoint;
    dxa::protocol::LobbyError error = dxa::protocol::LobbyError::WorkerUnavailable;
};

class IGameWorkerAllocator
{
public:
    virtual ~IGameWorkerAllocator() = default;
    [[nodiscard]] virtual WorkerAllocationResult Allocate(
        dxa::protocol::MatchId match,
        std::span<const dxa::protocol::PlayerId> players) = 0;
};

class UnavailableGameWorkerAllocator final : public IGameWorkerAllocator
{
public:
    [[nodiscard]] WorkerAllocationResult Allocate(
        dxa::protocol::MatchId,
        std::span<const dxa::protocol::PlayerId>) override;
};

class StaticGameWorkerAllocator final : public IGameWorkerAllocator
{
public:
    explicit StaticGameWorkerAllocator(GameEndpoint endpoint);
    [[nodiscard]] WorkerAllocationResult Allocate(
        dxa::protocol::MatchId,
        std::span<const dxa::protocol::PlayerId>) override;
};

struct OutboundMessage
{
    ConnectionId recipient;
    dxa::protocol::ServerMessage message;
};

enum class LobbyAuditEventType
{
    PlayerAssigned,
    RoomCreated,
    RoomDeleted,
    HostTransferred,
    MatchStarted,
    StartFailed
};

struct LobbyAuditEvent
{
    LobbyAuditEventType type = LobbyAuditEventType::PlayerAssigned;
    std::optional<ConnectionId> connection;
    std::optional<dxa::protocol::PlayerId> player;
    std::optional<dxa::protocol::RoomId> room;
    std::optional<dxa::protocol::MatchId> match;
    std::optional<dxa::protocol::LobbyError> error;
    std::optional<GameEndpoint> endpoint;
};

struct LobbyServiceResult
{
    std::vector<OutboundMessage> outbound;
    std::vector<LobbyAuditEvent> audit;
};

struct LobbyServiceConfig
{
    std::uint64_t nextConnection = 1U;
    std::uint32_t nextPlayer = 1U;
    std::uint32_t nextRoom = 1U;
    std::uint64_t nextMatch = 1U;
    std::uint64_t nextJoinOrdinal = 1U;
    std::uint32_t maximumRooms = 1024U;
};

class LobbyService
{
public:
    LobbyService(
        IGameWorkerAllocator& allocator,
        MatchTicketRegistry& tickets,
        LobbyServiceConfig config = {});
    [[nodiscard]] std::optional<ConnectionId> OpenConnection();
    [[nodiscard]] LobbyServiceResult Handle(
        ConnectionId connection,
        const dxa::protocol::ClientMessage& message,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] LobbyServiceResult Disconnect(ConnectionId connection);
    [[nodiscard]] MatchTicketRegistry& Tickets() noexcept;
    [[nodiscard]] std::size_t RoomCount() const noexcept;
    [[nodiscard]] bool HasRoom(dxa::protocol::RoomId room) const noexcept;
};
```

Store `ConnectionState { optional<PlayerId> player, uint32_t lastRequestId }`, `playerToConnection`, `playerToRoom`, and `rooms` maps. Validate connection and request sequence before visiting the message variant. Advance `lastRequestId` for every structurally valid new request, including semantic failure. Audit events never contain ticket values. Tests assert audit and outbound collections separately.

This task implements hello, list, create, join, leave, ready, and disconnect. `StartMatchRequest` returns `WorkerUnavailable` until Task 8 connects the start path.

- [ ] Step 4: Run GREEN and room tests

Run: `./scripts/build.ps1`

Run: `./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=LobbyService.*:LobbyRoom.*`

Expected: service and room tests pass.

- [ ] Step 5: Commit

```powershell
git add -- apps/lobby_server/CMakeLists.txt apps/lobby_server/include/dxa/lobby/GameWorkerAllocator.hpp apps/lobby_server/include/dxa/lobby/LobbyService.hpp apps/lobby_server/src/GameWorkerAllocator.cpp apps/lobby_server/src/LobbyService.cpp tests/CMakeLists.txt tests/lobby_service_test.cpp
git commit -m "feat(lobby): connection과 room service 추가" `
  -m "이유: TCP session과 무관한 한 aggregate에서 hello, request 순서와 room lifetime을 처리해야 했다." `
  -m "핵심 변경: ConnectionId mapping, strict request ID, room index, create와 join 및 leave와 ready broadcast를 구현했다." `
  -m "검증: LobbyService 헤더 부재 RED 뒤 handshake, duplicate request, room limit, disconnect와 host 승계 테스트를 통과했다."
```

---

### Task 8: Worker allocation and match start transaction

Files:

- Modify: `apps/lobby_server/include/dxa/lobby/GameWorkerAllocator.hpp`
- Modify: `apps/lobby_server/src/GameWorkerAllocator.cpp`
- Modify: `apps/lobby_server/src/LobbyService.cpp`
- Modify: `tests/lobby_service_test.cpp`

Interfaces:

- Consumes: Room start validation, MatchTicketRegistry.
- Produces: `GameEndpoint`, `WorkerAllocationResult`, start transaction and participant tickets.

- [ ] Step 1: Add start success and rollback RED tests

```cpp
TEST(LobbyService, WorkerFailureReturnsWaitingAndPreservesReadyState)
{
    LobbyFixture fixture{WorkerMode::Unavailable};
    const auto [host, guest, room] = ReadyTwoPlayerRoom(fixture);

    const auto output = fixture.service.Handle(
        host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));

    EXPECT_EQ(RoomState::Waiting, SnapshotFor(output, host).state);
    EXPECT_TRUE(Member(SnapshotFor(output, host), PlayerId{1U}).ready);
    EXPECT_TRUE(Member(SnapshotFor(output, guest), PlayerId{2U}).ready);
    EXPECT_EQ(LobbyError::WorkerUnavailable, ErrorFor(output, host).error);
}

TEST(LobbyService, StartIssuesOneDistinctTicketPerParticipant)
{
    LobbyFixture fixture{WorkerMode::Static};
    const auto [host, guest, room] = ReadyTwoPlayerRoom(fixture);
    const auto output = fixture.service.Handle(
        host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));

    const auto hostTicket = TicketFor(output, host);
    const auto guestTicket = TicketFor(output, guest);
    EXPECT_NE(hostTicket.ticket, guestTicket.ticket);
    EXPECT_EQ(5U, hostTicket.requestId);
    EXPECT_EQ(0U, guestTicket.requestId);
    EXPECT_EQ(RoomState::InMatch, SnapshotFor(output, host).state);
}
```

Add non-host, one-player, not-all-ready, invalid static endpoint, ticket source failure rollback, and MatchId exhaustion tests.

Extend `LobbyFixture` with a constructor that selects `UnavailableGameWorkerAllocator` or `StaticGameWorkerAllocator`. `ReadyTwoPlayerRoom` must welcome PlayerId 1 and 2, create RoomId 1, join the guest, set both ready with increasing request IDs, and return their ConnectionIds plus RoomId. `SnapshotFor`, `ErrorFor`, and `TicketFor` select by exact recipient ConnectionId and exact server message variant and throw when missing or duplicated.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: start success test receives `WorkerUnavailable` because the transaction is not connected.

- [ ] Step 3: Implement the atomic start path

```cpp
struct GameEndpoint
{
    std::string host;
    std::uint16_t tcpPort = 0;
    std::uint16_t udpPort = 0;
};

struct WorkerAllocationResult
{
    std::optional<GameEndpoint> endpoint;
    dxa::protocol::LobbyError error = dxa::protocol::LobbyError::WorkerUnavailable;
};

class IGameWorkerAllocator
{
public:
    virtual ~IGameWorkerAllocator() = default;
    [[nodiscard]] virtual WorkerAllocationResult Allocate(
        dxa::protocol::MatchId match,
        std::span<const dxa::protocol::PlayerId> players) = 0;
};
```

Run the transaction in this order: validate start, reserve MatchId, `BeginStarting`, issue every ticket into a temporary vector, allocate endpoint, then `MarkInMatch` and publish snapshots plus tickets. Any ticket or allocation failure calls `tickets.Revoke(temporaryTickets)`, calls `ReturnToWaiting`, leaves ready flags unchanged, and publishes Waiting snapshots before the host error. MatchId stays consumed because process IDs are never reused. Add a partial-source failure test that successfully issues the first ticket, fails the second, and proves the first ticket is no longer consumable.

- [ ] Step 4: Run GREEN and the full lobby core subset

Run: `./scripts/build.ps1`

Run: `./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=LobbyService.*:LobbyRoom.*:MatchTicketRegistry.*`

Expected: start success, rollback, room, and ticket tests pass.

- [ ] Step 5: Commit

```powershell
git add -- apps/lobby_server/include/dxa/lobby/GameWorkerAllocator.hpp apps/lobby_server/src/GameWorkerAllocator.cpp apps/lobby_server/src/LobbyService.cpp tests/lobby_service_test.cpp
git commit -m "feat(lobby): worker 배정과 match start 추가" `
  -m "이유: 방 조건 검증부터 participant ticket 전달까지 실패 시 복구 가능한 하나의 transaction이 필요했다." `
  -m "핵심 변경: static worker endpoint, MatchId 발급, participant별 ticket과 allocation failure rollback을 구현했다." `
  -m "검증: start가 WorkerUnavailable인 RED 뒤 성공 ticket, ready 보존 rollback, host와 인원 조건 테스트를 통과했다."
```

---

### Task 9: Async framed connection

Files:

- Create: `protocol/include/dxa/protocol/AsioFramedConnection.hpp`
- Create: `protocol/src/AsioFramedConnection.cpp`
- Create: `tests/protocol_asio_framed_connection_test.cpp`
- Modify: `protocol/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: `TcpFrameHeader`, `EncodedMessage`, frame codec.
- Produces: `RawFrame`, `AsioFramedConnection::Start`, `Send`, `Close`, `Socket`.

- [ ] Step 1: Write real loopback framed-connection RED tests

```cpp
TEST(AsioFramedConnection, ReadsFragmentedFrameAndPreservesWriteOrder)
{
    AsioSocketPair pair;
    std::vector<dxa::protocol::RawFrame> received;
    auto connection = dxa::protocol::AsioFramedConnection::Create(
        std::move(pair.server),
        [&](dxa::protocol::RawFrame frame) { received.push_back(std::move(frame)); },
        [&](boost::system::error_code) {});
    connection->Start();

    WriteInThreeFragments(pair.client, Frame(ClientHello{1U}));
    RunUntil(pair.io, [&] { return received.size() == 1U; });
    EXPECT_EQ(dxa::protocol::MessageType::ClientHello, received[0].type);

    connection->Send(MessageA());
    connection->Send(MessageB());
    EXPECT_EQ(ExpectedAThenB(), ReadTwoFrames(pair.client));
}
```

Define `AsioSocketPair` by binding a temporary acceptor to `127.0.0.1:0`, connecting the client socket, and accepting the server socket. `WriteInThreeFragments` writes bytes `[0, 3)`, `[3, 12)`, and the remaining payload. `RunUntil` uses a `steady_timer` that fails the test after five seconds. `ReadTwoFrames` decodes exactly two complete frames and returns their message types in arrival order.

Add tests for oversized header closing before payload read, truncated socket close, only one close callback, and pending write bytes above 256KiB closing the connection.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: FAIL because `AsioFramedConnection.hpp` does not exist.

- [ ] Step 3: Add the Boost.Asio target and async state machine

Use `find_package(Boost REQUIRED COMPONENTS asio)` and link `Boost::asio`. Add `BOOST_ASIO_NO_DEPRECATED` privately.

```cpp
struct RawFrame
{
    MessageType type = MessageType::ClientHello;
    std::vector<std::byte> payload;
};

class AsioFramedConnection final
    : public std::enable_shared_from_this<AsioFramedConnection>
{
public:
    using FrameHandler = std::function<void(RawFrame)>;
    using CloseHandler = std::function<void(boost::system::error_code)>;

    [[nodiscard]] static std::shared_ptr<AsioFramedConnection> Create(
        boost::asio::ip::tcp::socket socket,
        FrameHandler onFrame,
        CloseHandler onClose);
    void Start();
    [[nodiscard]] bool Send(const EncodedMessage& message);
    void Close();
    [[nodiscard]] boost::asio::ip::tcp::socket& Socket() noexcept;
};
```

Keep a fixed 12-byte header array, allocate the payload only after header success, and use `std::deque<std::vector<std::byte>>` for encoded frames. `Send` returns false and closes when pending bytes would exceed 262,144. Every handler captures `shared_from_this`.

- [ ] Step 4: Run GREEN

Run: `./scripts/build.ps1`

Run: `./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=AsioFramedConnection.*:TcpFrame.*`

Expected: connection and frame tests pass without a hang; each test has an internal five-second timer.

- [ ] Step 5: Commit

```powershell
git add -- protocol/CMakeLists.txt protocol/include/dxa/protocol/AsioFramedConnection.hpp protocol/src/AsioFramedConnection.cpp tests/CMakeLists.txt tests/protocol_asio_framed_connection_test.cpp
git commit -m "feat(network): 비동기 framed TCP connection 추가" `
  -m "이유: server와 client가 같은 header reader와 순서 보장 write queue를 공유해야 했다." `
  -m "핵심 변경: fragmented read, bounded payload, 256KiB pending queue와 단일 close callback을 구현했다." `
  -m "검증: connection 헤더 부재 RED 뒤 loopback fragment, write order, oversized frame과 slow client 테스트를 통과했다."
```

---

### Task 10: TCP lobby server adapter

Files:

- Create: `apps/lobby_server/include/dxa/lobby/LobbyTcpServer.hpp`
- Create: `apps/lobby_server/include/dxa/lobby/LobbyServerOptions.hpp`
- Create: `apps/lobby_server/src/LobbyTcpServer.cpp`
- Create: `apps/lobby_server/src/LobbyServerOptions.cpp`
- Create: `apps/lobby_server/src/main.cpp`
- Create: `tests/lobby_server_options_test.cpp`
- Create: `tests/support/lobby_network_fixture.hpp`
- Modify: `apps/lobby_server/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: `AsioFramedConnection`, client decode, server encode, LobbyService.
- Produces: `LobbyTcpServer`, `LobbyServerOptions`, `dxa_lobby_server`.

- [ ] Step 1: Write option and raw handshake RED tests

```cpp
TEST(LobbyServerOptions, DefaultsToLoopbackAndRejectsPartialWorkerEndpoint)
{
    const auto defaults = ParseLobbyServerOptions({});
    ASSERT_TRUE(defaults.options.has_value());
    EXPECT_EQ("127.0.0.1", defaults.options->bindAddress);
    EXPECT_EQ(7000U, defaults.options->port);

    const auto partial = ParseLobbyServerOptions({"--worker-host", "127.0.0.1"});
    EXPECT_FALSE(partial.options.has_value());
}
```

Lock the option types in the same task.

```cpp
struct LobbyServerOptions
{
    std::string bindAddress = "127.0.0.1";
    std::uint16_t port = 7000U;
    std::optional<GameEndpoint> worker;
};

struct LobbyServerOptionsParseResult
{
    std::optional<LobbyServerOptions> options;
    std::string error;
};

[[nodiscard]] LobbyServerOptionsParseResult ParseLobbyServerOptions(
    std::span<const std::string_view> arguments);
```

Create `tests/support/lobby_network_fixture.hpp` with a `RawLobbyServerFixture` that owns one server io_context, work guard, deterministic ticket source, ticket registry, selected allocator, LobbyService, LobbyTcpServer on port 0, and one server thread. Its destructor posts `LobbyTcpServer::Stop`, resets the work guard, and joins after the closed acceptor and sessions let the io_context drain. Do not call `io_context::stop` before the posted close runs. In `lobby_tcp_integration_test.cpp`, add this concrete test:

```cpp
TEST(LobbyTcpIntegration, RawHandshakeReturnsAssignedPlayer)
{
    RawLobbyServerFixture fixture{WorkerMode::Unavailable};
    boost::asio::io_context clientIo;
    boost::asio::ip::tcp::socket socket{clientIo};
    socket.connect({boost::asio::ip::make_address("127.0.0.1"), fixture.Port()});
    boost::asio::write(socket, boost::asio::buffer(Frame(ClientHello{1U})));

    const ServerMessage response = ReadOneServerMessage(socket);
    const auto* welcome = std::get_if<ServerWelcome>(&response);
    ASSERT_NE(nullptr, welcome);
    EXPECT_EQ(1U, welcome->requestId);
    EXPECT_EQ(PlayerId{1U}, welcome->player);
}
```

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: FAIL because server option and TCP server headers do not exist.

- [ ] Step 3: Implement accept, route, and cleanup

```cpp
class LobbyTcpServer
{
public:
    LobbyTcpServer(
        boost::asio::io_context& io,
        LobbyService& service,
        boost::asio::ip::tcp::endpoint endpoint);
    void Start();
    void Stop();
    [[nodiscard]] std::uint16_t LocalPort() const;
};
```

Each accepted session obtains an optional ConnectionId from `service.OpenConnection()`. If it is empty, close the newly accepted socket without creating a session. The TCP adapter logs connection open and close directly. Frame handlers decode only `ClientMessage`, call `service.Handle`, and route `result.outbound` through a `std::map<ConnectionId, weak_ptr<Session>>`. Close removes the session once, calls `Disconnect`, then routes host-transfer snapshots. Format `result.audit` through spdlog with connection, player, room, match, public error, and endpoint fields only. The audit type has no ticket field. Encode failure logs `InternalError` without ticket bytes and closes the offending session.

`LobbyTcpServer::Stop` closes the acceptor and every live session on the io_context thread. It is idempotent and does not call `io_context::stop`; the owner decides when the context may stop after close handlers drain.

`main.cpp` creates `SecureTicketSource`, registry, either static or unavailable allocator, service, server, spdlog logger, and `boost::asio::signal_set` for SIGINT and SIGTERM.

- [ ] Step 4: Run GREEN

Run: `./scripts/build.ps1`

Run: `./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=LobbyServerOptions.*:LobbyTcpIntegration.RawHandshake*`

Expected: parser and raw hello TCP test pass.

- [ ] Step 5: Commit

```powershell
git add -- apps/lobby_server/CMakeLists.txt apps/lobby_server/include/dxa/lobby/LobbyTcpServer.hpp apps/lobby_server/include/dxa/lobby/LobbyServerOptions.hpp apps/lobby_server/src/LobbyTcpServer.cpp apps/lobby_server/src/LobbyServerOptions.cpp apps/lobby_server/src/main.cpp tests/CMakeLists.txt tests/support/lobby_network_fixture.hpp tests/lobby_server_options_test.cpp tests/lobby_tcp_integration_test.cpp
git commit -m "feat(server): Boost.Asio lobby server 추가" `
  -m "이유: pure LobbyService를 실제 TCP session과 연결하고 disconnect broadcast를 같은 io_context에서 처리해야 했다." `
  -m "핵심 변경: loopback 기본 acceptor, connection routing, raw message decode, static worker option과 signal shutdown을 구현했다." `
  -m "검증: server header 부재 RED 뒤 option parser와 ephemeral-port raw hello integration을 통과했다."
```

---

### Task 11: Shared lobby client and two-player TCP completion

Files:

- Create: `apps/lobby_client/CMakeLists.txt`
- Create: `apps/lobby_client/include/dxa/lobby_client/LobbyClient.hpp`
- Create: `apps/lobby_client/src/LobbyClient.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/lobby_tcp_integration_test.cpp`
- Modify: `tests/support/lobby_network_fixture.hpp`

Interfaces:

- Consumes: `AsioFramedConnection`, lobby message codec.
- Produces: `LobbyClientCallbacks`, `LobbyClient::AsyncConnect`, typed send methods, `Close`.

- [ ] Step 1: Write a two-client end-to-end RED test

```cpp
TEST(LobbyTcpIntegration, TwoClientsCreateReadyStartAndReceiveDistinctTickets)
{
    LobbyNetworkFixture fixture{StaticEndpoint()};
    auto host = fixture.AddClient();
    auto guest = fixture.AddClient();

    fixture.ConnectAndWelcome(host);
    fixture.ConnectAndWelcome(guest);
    host->CreateRoom();
    fixture.RunUntil([&] { return fixture.HostRoom().has_value(); });
    guest->JoinRoom(*fixture.HostRoom());
    host->SetReady(true);
    guest->SetReady(true);
    host->StartMatch();

    fixture.RunUntil([&] { return fixture.HasTicket(host) && fixture.HasTicket(guest); });
    EXPECT_NE(fixture.Ticket(host).ticket, fixture.Ticket(guest).ticket);
    EXPECT_EQ(RoomState::InMatch, fixture.LatestRoom(host).state);
}
```

Add a worker-unavailable server test that observes Waiting snapshots and host error through `LobbyClient`, not direct service calls.

Extend `LobbyNetworkFixture` from `RawLobbyServerFixture` and give it a separate client io_context that is driven only by the test thread. `AddClient` installs callbacks that append every `ServerMessage` to a per-client vector on that thread, avoiding shared-vector races with the server thread. `ConnectAndWelcome` waits for `onConnected`, sends hello, and waits for one welcome. `RunUntil` polls the client io_context and fails through a five-second timer. `HostRoom`, `LatestRoom`, `HasTicket`, and `Ticket` select exact message variants without changing their order.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: FAIL because `dxa/lobby_client/LobbyClient.hpp` does not exist.

- [ ] Step 3: Implement the shared typed client

```cpp
struct LobbyClientCallbacks
{
    std::function<void()> onConnected;
    std::function<void(dxa::protocol::ServerMessage)> onMessage;
    std::function<void(boost::system::error_code)> onClosed;
};

class LobbyClient final : public std::enable_shared_from_this<LobbyClient>
{
public:
    [[nodiscard]] static std::shared_ptr<LobbyClient> Create(boost::asio::io_context& io);
    void AsyncConnect(std::string host, std::uint16_t port, LobbyClientCallbacks callbacks);
    [[nodiscard]] std::uint32_t Hello();
    [[nodiscard]] std::uint32_t ListRooms();
    [[nodiscard]] std::uint32_t CreateRoom();
    [[nodiscard]] std::uint32_t JoinRoom(dxa::protocol::RoomId room);
    [[nodiscard]] std::uint32_t LeaveRoom();
    [[nodiscard]] std::uint32_t SetReady(bool ready);
    [[nodiscard]] std::uint32_t StartMatch();
    void Close();
};
```

The client starts request IDs at 1 and checks overflow before increment. Typed send methods must run on the client io_context thread and queue directly into the framed connection. The CLI input thread posts its method call to that io_context; bot callbacks already run there. Server message decode failure closes with `boost::asio::error::invalid_argument`. Invoke each callback on the io_context thread.

- [ ] Step 4: Run GREEN and all network tests

Run: `./scripts/build.ps1`

Run: `./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=LobbyTcpIntegration.*:AsioFramedConnection.*`

Expected: raw handshake, two-player completion, worker failure, and framed connection tests pass.

- [ ] Step 5: Commit

```powershell
git add -- CMakeLists.txt apps/lobby_client/CMakeLists.txt apps/lobby_client/include/dxa/lobby_client/LobbyClient.hpp apps/lobby_client/src/LobbyClient.cpp tests/CMakeLists.txt tests/support/lobby_network_fixture.hpp tests/lobby_tcp_integration_test.cpp
git commit -m "feat(client): 공유 lobby TCP client 추가" `
  -m "이유: 사람용 CLI와 bot이 request sequence, codec과 socket lifecycle을 복제하지 않고 같은 경계를 사용해야 했다." `
  -m "핵심 변경: typed async request API와 callback decode를 구현하고 실제 두 client가 ticket까지 완주하도록 연결했다." `
  -m "검증: LobbyClient 헤더 부재 RED 뒤 two-client start, distinct ticket, worker failure와 framed TCP integration을 통과했다."
```

---

### Task 12: 24-player capacity and disconnect integration

Files:

- Modify: `tests/lobby_tcp_integration_test.cpp`
- Modify: `apps/lobby_server/src/LobbyTcpServer.cpp` only if RED exposes a session lifecycle defect.
- Modify: `apps/lobby_server/src/LobbyService.cpp` only if RED exposes a domain defect.

Interfaces:

- Consumes: complete server and shared client.
- Produces: executable evidence that 24 connections enter, the 25th fails, and host close transfers ownership.

- [ ] Step 1: Add capacity and disconnect RED tests

```cpp
TEST(LobbyTcpIntegration, AcceptsTwentyFourAndRejectsTwentyFifthConnectionFromRoom)
{
    LobbyNetworkFixture fixture{StaticEndpoint()};
    const auto clients = fixture.AddWelcomedClients(25U);
    clients[0]->CreateRoom();
    fixture.RunUntilRoomCreated();

    for (std::size_t index = 1; index < 24U; ++index)
    {
        clients[index]->JoinRoom(fixture.Room());
    }
    fixture.RunUntilMemberCount(24U);
    clients[24]->JoinRoom(fixture.Room());
    fixture.RunUntilError(clients[24], LobbyError::RoomFull);

    EXPECT_EQ(24U, fixture.LatestRoom(clients[0]).members.size());
}

TEST(LobbyTcpIntegration, ClosingHostSocketBroadcastsSuccessor)
{
    LobbyNetworkFixture fixture{StaticEndpoint()};
    const auto clients = fixture.AddWelcomedClients(3U);
    CreateAndJoinInOrder(fixture, clients);
    clients[0]->Close();
    fixture.RunUntilHost(PlayerId{2U});
    EXPECT_EQ(PlayerId{2U}, fixture.LatestRoom(clients[1]).host);
}
```

Add an oversized raw header test that records no new room and an internal five-second deadline for every network test.

- [ ] Step 2: Run RED or confirm the test detects an intentionally disabled path

Temporarily change the production `RoomCapacity` constant in `LobbyTypes.hpp` from 24 to 23 and run:

`./scripts/build.ps1`

`./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=LobbyTcpIntegration.AcceptsTwentyFourAndRejectsTwentyFifthConnectionFromRoom`

Expected: FAIL because the 24th client receives RoomFull. Restore the production constant to 24 immediately after recording RED and verify `git diff` contains no temporary constant change.

- [ ] Step 3: Make only the minimal lifecycle correction exposed by RED

If production already passes after restoring the locked capacity, make no production edit. Keep the test-only RED proof. If host close or oversized frame fails, trace and correct only the session removal or frame-close path named by the failing assertion.

- [ ] Step 4: Run GREEN and full network subset

Run: `./scripts/build.ps1`

Run: `./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=LobbyTcpIntegration.*:AsioFramedConnection.*:LobbyService.*`

Expected: all capacity, disconnect, frame, and service tests pass.

- [ ] Step 5: Commit

```powershell
git add -- tests/lobby_tcp_integration_test.cpp
git commit -m "test(lobby): 24인 TCP room 경계 고정" `
  -m "이유: domain unit test만으로는 실제 session 24개와 disconnect routing이 같은 결과를 내는지 증명할 수 없었다." `
  -m "핵심 변경: 24번째 입장, 25번째 거부, host socket close 승계와 과대 frame 무상태 검증을 추가했다." `
  -m "검증: capacity 23 fixture의 RED를 확인한 뒤 전체 lobby TCP integration을 통과했다."
```

If production files changed, add only those exact files to the same commit command and describe the reproduced defect in the body.

---

### Task 13: Console lobby client and 1-to-23 bot executable

Files:

- Create: `apps/lobby_cli/CMakeLists.txt`
- Create: `apps/lobby_cli/include/dxa/lobby_cli/LobbyCliCommand.hpp`
- Create: `apps/lobby_cli/include/dxa/lobby_cli/LobbyCliOutput.hpp`
- Create: `apps/lobby_cli/src/LobbyCliCommand.cpp`
- Create: `apps/lobby_cli/src/LobbyCliOutput.cpp`
- Create: `apps/lobby_cli/src/main.cpp`
- Create: `apps/bot_client/CMakeLists.txt`
- Create: `apps/bot_client/include/dxa/bot_client/BotClientOptions.hpp`
- Create: `apps/bot_client/src/BotClientOptions.cpp`
- Create: `apps/bot_client/src/main.cpp`
- Create: `tests/lobby_cli_command_test.cpp`
- Create: `tests/lobby_cli_output_test.cpp`
- Create: `tests/bot_client_options_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: `dxa_lobby_client` only for network access.
- Produces: `ParseLobbyCliCommand`, `FormatLobbyServerMessage`, `ParseBotClientOptions`, `dxa_lobby_cli`, `dxa_bot_client`.

- [ ] Step 1: Write parser RED tests

```cpp
TEST(LobbyCliCommand, ParsesEverySupportedCommandAndRejectsExtraTokens)
{
    EXPECT_EQ(LobbyCliCommandType::List, ParseLobbyCliCommand("list").command->type);
    EXPECT_EQ(RoomId{42U}, ParseLobbyCliCommand("join 42").command->room);
    EXPECT_TRUE(ParseLobbyCliCommand("ready on").command->ready);
    EXPECT_FALSE(ParseLobbyCliCommand("ready off").command->ready);
    EXPECT_FALSE(ParseLobbyCliCommand("start now").command.has_value());
}

TEST(BotClientOptions, AcceptsOneToTwentyThreeBots)
{
    EXPECT_EQ(1U, ParseBotClientOptions({"--room", "7", "--count", "1"}).options->count);
    EXPECT_EQ(23U, ParseBotClientOptions({"--room", "7", "--count", "23"}).options->count);
    EXPECT_FALSE(ParseBotClientOptions({"--room", "7", "--count", "24"}).options.has_value());
}

TEST(LobbyCliOutput, RedactsTicketBytes)
{
    MatchTicket ticket;
    ticket.requestId = 7U;
    ticket.match = MatchId{9U};
    ticket.ticket.fill(std::byte{0xAB});
    ticket.host = "127.0.0.1";
    ticket.tcpPort = 7100U;
    ticket.udpPort = 7101U;

    const std::string output = FormatLobbyServerMessage(ServerMessage{ticket});
    EXPECT_NE(std::string::npos, output.find("match ticket received"));
    EXPECT_NE(std::string::npos, output.find("match=9"));
    EXPECT_EQ(std::string::npos, output.find("AB"));
    EXPECT_EQ(std::string::npos, output.find("ab"));
}
```

Define the parser contracts explicitly.

```cpp
enum class LobbyCliCommandType { List, Create, Join, Leave, Ready, Start, Quit };

struct LobbyCliCommand
{
    LobbyCliCommandType type = LobbyCliCommandType::List;
    dxa::protocol::RoomId room;
    bool ready = false;
};

struct LobbyCliCommandParseResult
{
    std::optional<LobbyCliCommand> command;
    std::string error;
};

struct BotClientOptions
{
    std::string host = "127.0.0.1";
    std::uint16_t port = 7000U;
    dxa::protocol::RoomId room;
    std::uint32_t count = 1U;
};

struct BotClientOptionsParseResult
{
    std::optional<BotClientOptions> options;
    std::string error;
};

[[nodiscard]] LobbyCliCommandParseResult ParseLobbyCliCommand(
    std::string_view line);
[[nodiscard]] BotClientOptionsParseResult ParseBotClientOptions(
    std::span<const std::string_view> arguments);

[[nodiscard]] std::string FormatLobbyServerMessage(
    const dxa::protocol::ServerMessage& message);
```

Also test missing room, port 0, port 65,536, malformed unsigned values, and defaults host `127.0.0.1`, port 7000.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: FAIL because CLI and bot parser headers do not exist.

- [ ] Step 3: Implement parsers and apps through the shared client

The CLI input thread reads one line at a time, parses it, and posts exactly one typed `LobbyClient` call. Every callback passes its message through `FormatLobbyServerMessage`. The formatter prints PlayerId, room IDs, state, members, host, public errors, and the phrase `match ticket received` without bytes.

Each bot has this state machine:

```text
connected -> hello sent -> welcome -> join sent -> room snapshot -> ready sent
ready snapshot -> wait for match ticket -> success exit
```

`dxa_bot_client` creates `count` separate `LobbyClient` instances on one io_context. Exit 0 only when every bot receives a ticket. Exit 2 on option error, 3 on protocol error, and 4 after a 30-second steady timer. Never create a raw socket or call a protocol encoder from either app.

- [ ] Step 4: Run GREEN and full suite

Run: `./scripts/build.ps1`

Run: `./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=LobbyCliCommand.*:LobbyCliOutput.*:BotClientOptions.*`

Run: `./scripts/test.ps1`

Expected: parser tests and the entire Windows Debug suite pass.

- [ ] Step 5: Commit

```powershell
git add -- CMakeLists.txt apps/lobby_cli/CMakeLists.txt apps/lobby_cli/include/dxa/lobby_cli/LobbyCliCommand.hpp apps/lobby_cli/include/dxa/lobby_cli/LobbyCliOutput.hpp apps/lobby_cli/src/LobbyCliCommand.cpp apps/lobby_cli/src/LobbyCliOutput.cpp apps/lobby_cli/src/main.cpp apps/bot_client/CMakeLists.txt apps/bot_client/include/dxa/bot_client/BotClientOptions.hpp apps/bot_client/src/BotClientOptions.cpp apps/bot_client/src/main.cpp tests/CMakeLists.txt tests/lobby_cli_command_test.cpp tests/lobby_cli_output_test.cpp tests/bot_client_options_test.cpp
git commit -m "feat(client): lobby CLI와 23봇 client 추가" `
  -m "이유: 사람이 room command를 실행하고 bot 23개가 같은 LobbyClient transport로 join과 ready를 수행해야 했다." `
  -m "핵심 변경: console command parser, ticket redaction, 1부터 23 connection bot state machine과 timeout을 구현했다." `
  -m "검증: parser 헤더 부재 RED 뒤 CLI와 bot focused 테스트 및 Windows Debug 전체 suite를 통과했다."
```

---

### Task 14: Week 8 records, review, and PR

Files:

- Create: `docs/adr/0006-lobby-domain-and-tcp-adapter.md`
- Create: `docs/devlog/2026-08-24-lobby-room-flow.md`
- Modify: `README.md`
- Modify: `docs/PROJECT_PLAN.md`

Interfaces:

- Consumes: final code, actual test output, CI results, and reproduced failures.
- Produces: truthful Week 8 evidence and PR description.

- [ ] Step 1: Run the local human-visible scenario

Start the server with a static endpoint:

```powershell
./out/build/windows-msvc-vs-debug/apps/lobby_server/Debug/dxa_lobby_server.exe `
  --bind 127.0.0.1 `
  --port 7000 `
  --worker-host 127.0.0.1 `
  --worker-tcp-port 7100 `
  --worker-udp-port 7101
```

In a second terminal start the CLI, create a room, and note its RoomId:

```powershell
./out/build/windows-msvc-vs-debug/apps/lobby_cli/Debug/dxa_lobby_cli.exe `
  --host 127.0.0.1 `
  --port 7000
```

Run `create`, then start bots with the observed ID:

```powershell
./out/build/windows-msvc-vs-debug/apps/bot_client/Debug/dxa_bot_client.exe `
  --host 127.0.0.1 `
  --port 7000 `
  --room 1 `
  --count 23
```

The fresh server must report RoomId 1 after `create`; stop the scenario if it reports any other ID because the assumed clean process state is false. In the CLI run `ready on` and `start`. Record only public output: 24 members, InMatch, ticket received counts, exit codes, and elapsed wall time. Do not record ticket bytes.

- [ ] Step 2: Run final verification

Run: `./scripts/build.ps1`

Run: `./scripts/test.ps1`

Run: `./scripts/build.ps1 -Preset windows-msvc-release`

Expected: Windows Debug build, full tests, both WARP demos, lobby loopback tests, and MSVC Release build pass. Linux remains unverified locally unless a compiler is actually available; Ubuntu CI is the Linux gate.

- [ ] Step 3: Write records from actual evidence

ADR records why room domain, frame transport, and session routing are separate, why request IDs only increase, why static worker is not a real game server, and why no mutex is used in the one-thread v1 service.

Devlog order is symptom, reproduction, hypothesis, alternative, implementation, result, limitation. Include only failures actually observed during TDD or CI. Include exact test counts and the actual 24-client scenario output. State that game server ticket consumption is unit-tested but not yet connected over a live game server.

README adds three commands for server, CLI, and bot. It states Week 8 implements real lobby TCP while UDP and authoritative network gameplay remain absent. Project plan marks Week 8 complete and Week 9 next only after verification passes.

- [ ] Step 4: Review the full diff from the Week 7 merge

Review base: `2e3bec8628be9a7e0490484a5de61da4ccac36ab`.

Inspect correctness, protocol bounds, ticket secrecy, request replay, disconnect lifecycle, async ownership, Windows and Linux portability, and test false-pass paths. This repository keeps the review local and does not use external or subagent review unless the user explicitly changes that boundary.

For each reproduced finding, add a focused failing test, apply one fix, rerun the focused test, and commit separately with a `fix(review):` or `fix(ci):` subject and 이유, 핵심 변경, 검증 body.

- [ ] Step 5: Commit Week 8 records

```powershell
git add -- README.md docs/PROJECT_PLAN.md docs/adr/0006-lobby-domain-and-tcp-adapter.md docs/devlog/2026-08-24-lobby-room-flow.md
git commit -m "docs(network): 8주차 lobby TCP 완주 기록" `
  -m "이유: room 생성부터 participant ticket까지 실제 TCP 결과와 남은 game server 경계를 재현 가능한 기록으로 남겨야 했다." `
  -m "핵심 변경: ADR, 개발 기록, 실행 명령, test 결과와 프로젝트 진행 상태를 실제 evidence에 맞춰 갱신했다." `
  -m "검증: 문서 링크, 명령, test count, ticket redaction, placeholder와 git diff whitespace를 확인했다."
```

- [ ] Step 6: Push and open the milestone PR

Push `feat/lobby-room-flow` and open a PR against `main`. Fill every heading in `.github/PULL_REQUEST_TEMPLATE.md`. Use a merge commit later and never squash. The PR body separates completed behavior, measured evidence, CI-only facts, and deferred Week 9 scope. Monitor Windows and Ubuntu CI until merge-ready. Do not merge without a new user instruction.

---

## Expected Commit Sequence

1. `feat(protocol): 로비 ID와 wire enum 추가`
2. `feat(protocol): bounded little-endian codec 추가`
3. `feat(protocol): 64KiB TCP frame 경계 추가`
4. `feat(protocol): 로비 message codec 추가`
5. `feat(lobby): 24인 room state machine 추가`
6. `feat(lobby): 128비트 일회용 ticket 추가`
7. `feat(lobby): connection과 room service 추가`
8. `feat(lobby): worker 배정과 match start 추가`
9. `feat(network): 비동기 framed TCP connection 추가`
10. `feat(server): Boost.Asio lobby server 추가`
11. `feat(client): 공유 lobby TCP client 추가`
12. `test(lobby): 24인 TCP room 경계 고정`
13. `feat(client): lobby CLI와 23봇 client 추가`
14. `docs(network): 8주차 lobby TCP 완주 기록`

Review and CI fixes are added only when a failure is reproduced. Commit counts are not targets, dates are not rewritten, and no synthetic incident or measurement is created.
