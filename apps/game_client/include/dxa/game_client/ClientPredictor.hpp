#pragma once

#include <dxa/simulation/MatchTypes.hpp>
#include <dxa/simulation/NavMesh.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>

namespace dxa::game_client
{
class PredictionSynchronizationError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

struct PredictedInput
{
    std::uint32_t sequence = 0U;
    std::optional<dxa::simulation::Vec2> moveDestination;
    std::optional<dxa::simulation::ActorId> attackTarget;
};

class ClientPredictor
{
public:
    ClientPredictor(
        dxa::simulation::NavMesh navMesh,
        dxa::simulation::Vec2 position,
        float speed,
        float stoppingDistance);
    ~ClientPredictor();
    ClientPredictor(ClientPredictor&&) noexcept;
    ClientPredictor& operator=(ClientPredictor&&) noexcept;
    ClientPredictor(const ClientPredictor&) = delete;
    ClientPredictor& operator=(const ClientPredictor&) = delete;

    [[nodiscard]] bool SetDestination(dxa::simulation::Vec2 destination);
    [[nodiscard]] PredictedInput AdvanceTick();
    void Reconcile(
        dxa::simulation::Vec2 authoritativePosition,
        std::uint32_t acknowledgedSequence);
    [[nodiscard]] dxa::simulation::Vec2 Position() const noexcept;
    [[nodiscard]] std::size_t HistorySize() const noexcept;
    [[nodiscard]] std::uint32_t LastIssuedSequence() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace dxa::game_client
