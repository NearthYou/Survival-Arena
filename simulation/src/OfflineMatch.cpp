#include <dxa/simulation/OfflineMatch.hpp>

#include "OfflineMatchInternal.hpp"

#include <dxa/simulation/SafeZone.hpp>

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dxa::simulation
{
OfflineMatch OfflineMatch::Create(const NavMesh& navMesh, MatchConfig config)
{
    ValidateMatchConfig(config);
    return OfflineMatch{std::make_unique<Impl>(navMesh, std::move(config))};
}

OfflineMatch::OfflineMatch(std::unique_ptr<Impl> impl) noexcept
    : impl_{std::move(impl)}
{
}

OfflineMatch::~OfflineMatch() = default;
OfflineMatch::OfflineMatch(OfflineMatch&&) noexcept = default;
OfflineMatch& OfflineMatch::operator=(OfflineMatch&&) noexcept = default;

void OfflineMatch::Start()
{
    if (impl_ == nullptr)
    {
        throw std::logic_error{"offline match has been moved from"};
    }
    if (impl_->phase != MatchPhase::Waiting)
    {
        throw std::logic_error{"offline match can start only once"};
    }
    impl_->Spawn();
    impl_->phase = MatchPhase::Running;
}

void OfflineMatch::Submit(MatchCommand command)
{
    if (impl_ == nullptr)
    {
        throw std::logic_error{"offline match has been moved from"};
    }
    if (impl_->phase != MatchPhase::Running)
    {
        throw std::logic_error{"offline match accepts commands only while running"};
    }
    impl_->queuedCommands.insert_or_assign(command.actor, std::move(command));
}

void OfflineMatch::Step()
{
    if (impl_ == nullptr)
    {
        throw std::logic_error{"offline match has been moved from"};
    }
    if (impl_->phase != MatchPhase::Running)
    {
        throw std::logic_error{"offline match can step only while running"};
    }
}

MatchSnapshot OfflineMatch::Snapshot() const
{
    if (impl_ == nullptr)
    {
        throw std::logic_error{"offline match has been moved from"};
    }
    if (impl_->actors.size() != impl_->neutralArchetypes.size())
    {
        throw std::logic_error{"offline match actor metadata is inconsistent"};
    }

    const SafeZoneState zone = EvaluateSafeZone(impl_->tick, impl_->config.tickRate);
    MatchSnapshot snapshot;
    snapshot.tick = impl_->tick;
    snapshot.elapsedSeconds = static_cast<double>(impl_->tick)
        / static_cast<double>(impl_->config.tickRate);
    snapshot.phase = impl_->phase;
    snapshot.safeZoneStage = zone.stage;
    snapshot.safeZoneCenter = zone.center;
    snapshot.safeZoneRadius = zone.radius;
    snapshot.result = impl_->result;
    snapshot.eventChecksum = impl_->eventChecksum;
    snapshot.actors.reserve(impl_->actors.size());
    snapshot.loot.reserve(impl_->loot.size());

    for (std::size_t index = 0; index < impl_->actors.size(); ++index)
    {
        const CombatActor& actor = impl_->actors[index];
        snapshot.actors.push_back(ActorSnapshot{
            actor.id,
            actor.role,
            impl_->neutralArchetypes[index],
            actor.position,
            actor.health,
            actor.alive,
            actor.weapon,
            actor.cooldownTicksRemaining,
            actor.eliminations});
        if (actor.role == ActorRole::Contender && actor.alive)
        {
            ++snapshot.aliveContenders;
        }
    }
    for (const LootItem& item : impl_->loot)
    {
        snapshot.loot.push_back(
            LootSnapshot{item.id, item.type, item.position, item.active});
    }
    return snapshot;
}

std::vector<MatchEvent> OfflineMatch::DrainEvents()
{
    if (impl_ == nullptr)
    {
        throw std::logic_error{"offline match has been moved from"};
    }
    std::vector<MatchEvent> drained;
    drained.swap(impl_->events);
    return drained;
}
} // namespace dxa::simulation
