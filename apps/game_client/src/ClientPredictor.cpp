#include <dxa/game_client/ClientPredictor.hpp>

#include <dxa/protocol/GameTypes.hpp>
#include <dxa/simulation/Math2.hpp>
#include <dxa/simulation/NavAgent.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dxa::game_client
{
struct ClientPredictor::Impl
{
    Impl(
        dxa::simulation::NavMesh sourceMesh,
        const dxa::simulation::Vec2 position,
        const float sourceSpeed,
        const float sourceStoppingDistance)
        : navMesh{std::move(sourceMesh)},
          speed{sourceSpeed},
          stoppingDistance{sourceStoppingDistance},
          agent{std::make_unique<dxa::simulation::NavAgent>(
              navMesh,
              position,
              speed,
              stoppingDistance)}
    {
    }

    void Apply(const PredictedInput& input)
    {
        if (input.moveDestination.has_value())
        {
            if (!agent->SetDestination(*input.moveDestination))
            {
                throw PredictionSynchronizationError{
                    "stored prediction destination became invalid"};
            }
        }
        agent->Tick(1.0F / static_cast<float>(dxa::protocol::GameTickRate));
    }

    dxa::simulation::NavMesh navMesh;
    float speed = 0.0F;
    float stoppingDistance = 0.0F;
    std::unique_ptr<dxa::simulation::NavAgent> agent;
    std::optional<dxa::simulation::Vec2> desiredDestination;
    std::vector<PredictedInput> history;
    std::uint32_t lastIssuedSequence = 0U;
    std::uint32_t lastAcknowledgedSequence = 0U;
};

ClientPredictor::ClientPredictor(
    dxa::simulation::NavMesh navMesh,
    const dxa::simulation::Vec2 position,
    const float speed,
    const float stoppingDistance)
    : impl_{std::make_unique<Impl>(
          std::move(navMesh),
          position,
          speed,
          stoppingDistance)}
{
}

ClientPredictor::~ClientPredictor() = default;
ClientPredictor::ClientPredictor(ClientPredictor&&) noexcept = default;
ClientPredictor& ClientPredictor::operator=(ClientPredictor&&) noexcept =
    default;

bool ClientPredictor::SetDestination(
    const dxa::simulation::Vec2 destination)
{
    if (impl_ == nullptr
        || !dxa::simulation::IsFinite(destination)
        || !impl_->navMesh.FindPath(
                impl_->agent->Position(),
                destination).has_value())
    {
        return false;
    }
    if (!impl_->agent->SetDestination(destination))
    {
        return false;
    }
    impl_->desiredDestination = destination;
    return true;
}

PredictedInput ClientPredictor::AdvanceTick()
{
    if (impl_ == nullptr)
    {
        throw PredictionSynchronizationError{"predictor has been moved from"};
    }
    if (impl_->history.size() >= dxa::protocol::MaxClientInputHistory)
    {
        throw PredictionSynchronizationError{
            "prediction input history capacity exceeded"};
    }
    if (impl_->lastIssuedSequence
        == std::numeric_limits<std::uint32_t>::max())
    {
        throw PredictionSynchronizationError{
            "prediction input sequence exhausted"};
    }

    PredictedInput input;
    input.sequence = ++impl_->lastIssuedSequence;
    input.moveDestination = impl_->desiredDestination;
    impl_->history.push_back(input);
    impl_->Apply(input);
    return input;
}

void ClientPredictor::Reconcile(
    const dxa::simulation::Vec2 authoritativePosition,
    const std::uint32_t acknowledgedSequence)
{
    if (impl_ == nullptr)
    {
        throw PredictionSynchronizationError{"predictor has been moved from"};
    }
    if (acknowledgedSequence > impl_->lastIssuedSequence)
    {
        throw PredictionSynchronizationError{
            "snapshot ACK exceeds issued prediction sequence"};
    }
    if (acknowledgedSequence < impl_->lastAcknowledgedSequence)
    {
        return;
    }

    auto reconciled = std::make_unique<dxa::simulation::NavAgent>(
        impl_->navMesh,
        authoritativePosition,
        impl_->speed,
        impl_->stoppingDistance);
    impl_->history.erase(
        std::remove_if(
            impl_->history.begin(),
            impl_->history.end(),
            [acknowledgedSequence](const PredictedInput& input) {
                return input.sequence <= acknowledgedSequence;
            }),
        impl_->history.end());
    impl_->agent = std::move(reconciled);
    impl_->lastAcknowledgedSequence = acknowledgedSequence;
    for (const PredictedInput& input : impl_->history)
    {
        impl_->Apply(input);
    }
}

dxa::simulation::Vec2 ClientPredictor::Position() const noexcept
{
    return impl_ == nullptr
        ? dxa::simulation::Vec2{}
        : impl_->agent->Position();
}

std::size_t ClientPredictor::HistorySize() const noexcept
{
    return impl_ == nullptr ? 0U : impl_->history.size();
}

std::uint32_t ClientPredictor::LastIssuedSequence() const noexcept
{
    return impl_ == nullptr ? 0U : impl_->lastIssuedSequence;
}
} // namespace dxa::game_client
