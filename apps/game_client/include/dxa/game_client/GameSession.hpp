#pragma once

#include <dxa/game_common/NetworkMetrics.hpp>
#include <dxa/protocol/GameSnapshot.hpp>
#include <dxa/protocol/GameTcpMessages.hpp>
#include <dxa/protocol/LobbyMessages.hpp>
#include <dxa/simulation/Math2.hpp>
#include <dxa/simulation/NavMesh.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace dxa::game_client
{
class GameNetworkRuntime;

struct GameSessionStart
{
    dxa::protocol::PlayerId player;
    dxa::protocol::MatchTicket ticket;
    std::uint32_t expectedMapId = 1U;
    std::uint32_t expectedNavMeshCrc32 = 0U;
};

enum class GameSessionState
{
    Idle,
    Connecting,
    Authenticating,
    BindingUdp,
    Synchronizing,
    Running,
    Finished,
    ProtocolError,
    Closed
};

struct GameSceneFrame
{
    bool connected = false;
    dxa::protocol::EntityId localActor;
    bool localAlive = false;
    dxa::simulation::Vec2 localPosition;
    std::vector<dxa::protocol::NetworkActorSnapshot> actors;
    float zoneRadius = 128.0F;
    std::uint32_t lastAckInputSequence = 0U;
    std::uint64_t snapshotCount = 0U;
};

class GameSession
{
public:
    explicit GameSession(dxa::simulation::NavMesh navMesh);
    GameSession(
        dxa::simulation::NavMesh navMesh,
        std::shared_ptr<GameNetworkRuntime> runtime);
    ~GameSession();
    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

    void Start(GameSessionStart start);
    [[nodiscard]] bool SetDestination(
        dxa::simulation::Vec2 destination);
    void FixedUpdate();
    [[nodiscard]] GameSceneFrame SampleScene() const;
    [[nodiscard]] GameSessionState State() const noexcept;
    [[nodiscard]] std::optional<dxa::protocol::GameMatchResult>
    Result() const;
    [[nodiscard]] std::uint64_t SnapshotCount() const noexcept;
    [[nodiscard]] dxa::game_common::GameSessionMetrics Metrics() const;
    void Stop();

private:
    struct Impl;
    std::shared_ptr<GameNetworkRuntime> runtime_;
    bool ownsRuntime_ = false;
    std::shared_ptr<Impl> impl_;
};
} // namespace dxa::game_client
