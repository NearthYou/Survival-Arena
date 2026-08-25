#pragma once

#include <dxa/protocol/GameTypes.hpp>
#include <dxa/protocol/Ids.hpp>

#include <cstdint>
#include <variant>
#include <vector>

namespace dxa::protocol
{
struct NetworkVec2
{
    float x = 0.0F;
    float z = 0.0F;

    [[nodiscard]] bool operator==(const NetworkVec2&) const = default;
};

struct UdpBind
{
    MatchId match;
    PlayerId player;
    UdpSessionToken token;

    [[nodiscard]] bool operator==(const UdpBind&) const = default;
};

struct UdpBindAccepted
{
    MatchId match;
    PlayerId player;
    std::uint32_t serverTick = 0U;

    [[nodiscard]] bool operator==(const UdpBindAccepted&) const = default;
};

struct ClientInput
{
    MatchId match;
    PlayerId player;
    UdpSessionToken token;
    std::uint32_t inputSequence = 0U;
    NetworkVec2 moveDestination;
    bool hasMoveDestination = false;
    EntityId attackTarget;
    bool hasAttackTarget = false;

    [[nodiscard]] bool operator==(const ClientInput&) const = default;
};

struct SnapshotFragment
{
    MatchId match;
    std::uint32_t snapshotId = 0U;
    std::uint32_t serverTick = 0U;
    std::uint32_t ackInputSequence = 0U;
    std::uint16_t fragmentIndex = 0U;
    std::uint16_t fragmentCount = 0U;
    std::uint32_t fullPayloadBytes = 0U;
    std::uint32_t fullPayloadCrc32 = 0U;
    std::vector<std::byte> bytes;

    [[nodiscard]] bool operator==(const SnapshotFragment&) const = default;
};

using ClientDatagram = std::variant<UdpBind, ClientInput>;
using ServerDatagram = std::variant<UdpBindAccepted, SnapshotFragment>;

struct EncodedDatagram
{
    UdpDatagramType type = UdpDatagramType::Bind;
    std::vector<std::byte> bytes;

    [[nodiscard]] bool operator==(const EncodedDatagram&) const = default;
};
} // namespace dxa::protocol
