#pragma once

#include <dxa/game_server/ParticipantRoster.hpp>
#include <dxa/game_server/ServerMatchMetrics.hpp>
#include <dxa/game_server/SnapshotReplicator.hpp>
#include <dxa/game_server/UdpTokenSource.hpp>

#include <dxa/protocol/GameSnapshot.hpp>
#include <dxa/protocol/GameTcpMessages.hpp>
#include <dxa/protocol/GameUdpMessages.hpp>
#include <dxa/protocol/WorkerControlMessages.hpp>
#include <dxa/simulation/ArenaMap.hpp>
#include <dxa/simulation/MatchConfig.hpp>

#include <array>
#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace dxa::game_server
{
struct UdpPeer
{
    std::array<std::byte, 16U> address{};
    std::uint16_t port = 0U;
    bool ipv6 = false;

    [[nodiscard]] auto operator<=>(const UdpPeer&) const = default;
};

struct GameTcpOutbound
{
    GameConnectionId recipient;
    dxa::protocol::GameServerMessage message;
    bool closeAfterWrite = false;
};

struct GameUdpOutbound
{
    UdpPeer recipient;
    dxa::protocol::ServerDatagram datagram;
    std::uint64_t shapingPeerKey = 0U;
};

struct AuthoritativeMatchResult
{
    std::vector<GameTcpOutbound> tcp;
    std::vector<GameUdpOutbound> udp;
    std::vector<dxa::protocol::WorkerToLobbyMessage> control;
    std::vector<GameConnectionId> closeTcp;
    std::uint32_t ticksExecuted = 0U;
    bool overrun = false;
    std::chrono::steady_clock::duration overrunLateness{};
    std::uint64_t totalOverruns = 0U;
};

class AuthoritativeMatch
{
public:
    [[nodiscard]] static AuthoritativeMatch Create(
        const dxa::protocol::ReserveMatch& reservation,
        const dxa::simulation::ArenaMapDefinition& arena,
        dxa::simulation::MatchConfig config,
        IUdpTokenSource& tokenSource,
        std::chrono::steady_clock::time_point now,
        ReplicationConfig replication = {});

    ~AuthoritativeMatch();
    AuthoritativeMatch(AuthoritativeMatch&&) noexcept;
    AuthoritativeMatch& operator=(AuthoritativeMatch&&) noexcept;
    AuthoritativeMatch(const AuthoritativeMatch&) = delete;
    AuthoritativeMatch& operator=(const AuthoritativeMatch&) = delete;

    [[nodiscard]] AuthoritativeMatchResult Authenticate(
        GameConnectionId connection,
        const dxa::protocol::GameClientHello& hello,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] AuthoritativeMatchResult ReceiveClientDatagram(
        UdpPeer peer,
        const dxa::protocol::ClientDatagram& datagram);
    [[nodiscard]] AuthoritativeMatchResult Disconnect(
        GameConnectionId connection);
    [[nodiscard]] AuthoritativeMatchResult Advance(
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::optional<std::chrono::steady_clock::time_point>
    NextDeadline() const;
    [[nodiscard]] bool Started() const noexcept;
    [[nodiscard]] dxa::protocol::GameSnapshot Snapshot() const;
    [[nodiscard]] ServerMatchMetricsSnapshot Metrics(
        dxa::game_common::GameTrafficTotals traffic = {}) const;

private:
    struct Impl;

    explicit AuthoritativeMatch(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};
} // namespace dxa::game_server
