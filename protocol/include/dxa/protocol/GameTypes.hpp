#pragma once

#include <dxa/protocol/LobbyTypes.hpp>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace dxa::protocol
{
namespace detail
{
template <typename Tag>
class Opaque128 final
{
public:
    constexpr Opaque128() noexcept = default;

    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return bytes_.size();
    }

    constexpr void fill(const std::byte value) noexcept
    {
        bytes_.fill(value);
    }

    [[nodiscard]] constexpr std::byte& operator[](
        const std::size_t index) noexcept
    {
        return bytes_[index];
    }

    [[nodiscard]] constexpr const std::byte& operator[](
        const std::size_t index) const noexcept
    {
        return bytes_[index];
    }

    [[nodiscard]] constexpr auto begin() noexcept
    {
        return bytes_.begin();
    }

    [[nodiscard]] constexpr auto begin() const noexcept
    {
        return bytes_.begin();
    }

    [[nodiscard]] constexpr auto end() noexcept
    {
        return bytes_.end();
    }

    [[nodiscard]] constexpr auto end() const noexcept
    {
        return bytes_.end();
    }

    [[nodiscard]] constexpr operator std::span<
        std::byte,
        MatchTicketBytes>() noexcept
    {
        return bytes_;
    }

    [[nodiscard]] constexpr operator std::span<
        const std::byte,
        MatchTicketBytes>() const noexcept
    {
        return bytes_;
    }

    [[nodiscard]] auto operator<=>(const Opaque128&) const = default;

private:
    std::array<std::byte, MatchTicketBytes> bytes_{};
};

struct MatchTicketTag final
{
};

struct UdpSessionTokenTag final
{
};
} // namespace detail

using MatchTicketValue = detail::Opaque128<detail::MatchTicketTag>;
using UdpSessionToken = detail::Opaque128<detail::UdpSessionTokenTag>;

inline constexpr std::uint16_t GameTickRate = 30U;
inline constexpr std::uint16_t SnapshotRate = 15U;
inline constexpr std::size_t UdpHeaderBytes = 10U;
inline constexpr std::size_t MaxUdpDatagramBytes = 1200U;
inline constexpr std::size_t SnapshotFragmentMetadataBytes = 32U;
inline constexpr std::size_t MaxSnapshotFragmentPayloadBytes = 1158U;
inline constexpr std::size_t MaxSnapshotFragments = 32U;
inline constexpr std::size_t MaxSnapshotPayloadBytes = 37056U;
inline constexpr std::size_t MaxClientInputHistory = 256U;
inline constexpr std::size_t MaxClientSnapshotBuffer = 32U;

enum class ReplicationMode : std::uint8_t
{
    FullState = 1,
    InterestFullPrecision = 2,
    InterestQuantized = 3,
    InterestDelta = 4
};

struct GameEndpoint
{
    std::string host;
    std::uint16_t tcpPort = 0U;
    std::uint16_t udpPort = 0U;

    [[nodiscard]] bool operator==(const GameEndpoint&) const = default;
};

enum class WorkerReservationReject : std::uint8_t
{
    Busy = 1,
    InvalidReservation = 2,
    SimulationInitializationFailed = 3,
    InternalError = 4
};

enum class GameServerErrorCode : std::uint8_t
{
    AuthenticationFailed = 1,
    ServerNotReady = 2,
    ProtocolViolation = 3,
    InternalError = 4
};

enum class MatchCompletionReason : std::uint8_t
{
    LastSurvivor = 1,
    TimeLimit = 2,
    NoAuthenticatedPlayers = 3,
    NoConnectedPlayers = 4
};

enum class UdpDatagramType : std::uint8_t
{
    Bind = 1,
    BindAccepted = 2,
    ClientInput = 3,
    SnapshotFragment = 4
};
} // namespace dxa::protocol
