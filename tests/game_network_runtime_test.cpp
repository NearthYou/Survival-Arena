#include <dxa/game_client/GameNetworkRuntime.hpp>
#include <dxa/game_client/GameSession.hpp>
#include <dxa/simulation/ArenaMap.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <utility>

TEST(GameNetworkRuntime, StartsAndStopsIdempotently)
{
    dxa::game_client::GameNetworkRuntime runtime;

    EXPECT_TRUE(runtime.Start());
    EXPECT_FALSE(runtime.Start());
    runtime.Stop();
    runtime.Stop();
    EXPECT_FALSE(runtime.Start());
}

TEST(GameNetworkRuntime, SharedSessionRequiresStartedRuntime)
{
    const auto createSession = [](
        std::shared_ptr<dxa::game_client::GameNetworkRuntime> runtime) {
        return std::make_unique<dxa::game_client::GameSession>(
            dxa::simulation::BuildSurvivalArenaNavMesh(),
            std::move(runtime));
    };

    EXPECT_THROW(
        (void)createSession({}),
        std::invalid_argument);

    auto runtime = std::make_shared<dxa::game_client::GameNetworkRuntime>();
    EXPECT_THROW(
        (void)createSession(runtime),
        std::logic_error);
}
