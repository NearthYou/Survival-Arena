#pragma once

#include <dxa/simulation/Combat.hpp>
#include <dxa/simulation/Loot.hpp>
#include <dxa/simulation/NavAgent.hpp>
#include <dxa/simulation/OfflineBotController.hpp>
#include <dxa/simulation/OfflineMatch.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace dxa::simulation
{
struct OfflineMatch::Impl
{
    Impl(const NavMesh& sourceNavMesh, MatchConfig sourceConfig)
        : navMesh{sourceNavMesh},
          config{std::move(sourceConfig)}
    {
    }

    void Spawn();
    void Step();
    void RecordEvents(std::vector<MatchEvent> tickEvents);
    [[nodiscard]] std::vector<MatchCommand> BuildInternalBotCommands() const;

    NavMesh navMesh;
    MatchConfig config;
    MatchPhase phase = MatchPhase::Waiting;
    std::uint32_t tick = 0;
    std::vector<CombatActor> actors;
    std::vector<NeutralArchetype> neutralArchetypes;
    std::vector<NavAgent> agents;
    std::vector<std::unique_ptr<BehaviorTreeAiController>> neutralControllers;
    std::vector<LootItem> loot;
    std::vector<MatchCommand> queuedCommands;
    std::vector<MatchLifecycleCommand> queuedLifecycleCommands;
    std::map<ActorId, MatchCommand> selectedCommands;
    std::vector<MatchEvent> events;
    std::optional<MatchResult> result;
    std::uint64_t eventChecksum = 14695981039346656037ULL;
};
} // namespace dxa::simulation
