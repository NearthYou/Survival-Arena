#pragma once

#include <dxa/protocol/Ids.hpp>
#include <dxa/protocol/LobbyTypes.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace dxa::lobby
{
struct GameEndpoint
{
    std::string host;
    std::uint16_t tcpPort = 0;
    std::uint16_t udpPort = 0;

    [[nodiscard]] bool operator==(const GameEndpoint&) const = default;
};

struct WorkerAllocationResult
{
    std::optional<GameEndpoint> endpoint;
    dxa::protocol::LobbyError error =
        dxa::protocol::LobbyError::WorkerUnavailable;
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

private:
    GameEndpoint endpoint_;
    bool valid_ = false;
};
} // namespace dxa::lobby
