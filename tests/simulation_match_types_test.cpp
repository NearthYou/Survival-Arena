#include <dxa/simulation/MatchConfig.hpp>
#include <dxa/simulation/MatchTypes.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace
{
using dxa::simulation::ActorSnapshot;
using dxa::simulation::DefaultMatchConfig;
using dxa::simulation::MatchConfig;
using dxa::simulation::MatchEndReason;
using dxa::simulation::MatchEvent;
using dxa::simulation::MatchEventType;
using dxa::simulation::MatchPhase;
using dxa::simulation::MatchResult;
using dxa::simulation::MatchSnapshot;
using dxa::simulation::NeutralArchetype;
using dxa::simulation::ValidateMatchConfig;
using dxa::simulation::WeaponType;

TEST(MatchConfig, LocksCanonicalPopulationTimingAndMovementDefaults)
{
    const MatchConfig config = DefaultMatchConfig();

    EXPECT_EQ(30U, config.tickRate);
    EXPECT_EQ(24U, config.contenderCount);
    EXPECT_EQ(50U, config.meleeNeutralCount);
    EXPECT_EQ(50U, config.rangedNeutralCount);
    EXPECT_EQ(24U, config.rifleLootCount);
    EXPECT_EQ(12U, config.arcPulseLootCount);
    EXPECT_EQ(24U, config.medKitLootCount);
    EXPECT_EQ(6U, config.botDecisionIntervalTicks);
    EXPECT_TRUE(config.enableInternalBots);
    EXPECT_EQ(4096U, config.maximumSpawnAttempts);
    EXPECT_EQ(14400U, config.suddenDeathTick);
    EXPECT_EQ(18000U, config.hardTimeoutTick);
    EXPECT_EQ(20260823U, config.seed);
    EXPECT_FLOAT_EQ(6.0F, config.contenderSpeed);
    EXPECT_FLOAT_EQ(4.5F, config.neutralSpeed);
    EXPECT_FLOAT_EQ(18.0F, config.contenderPerceptionRadius);
    EXPECT_FLOAT_EQ(10.0F, config.neutralPerceptionRadius);
    EXPECT_FLOAT_EQ(1.0F, config.pickupRadius);
    EXPECT_FLOAT_EQ(3.0F, config.contenderSpawnSpacing);
    EXPECT_FLOAT_EQ(0.75F, config.neutralSpawnSpacing);
    EXPECT_FLOAT_EQ(128.0F, config.arenaHalfExtent);
    EXPECT_FLOAT_EQ(80.0F, config.contenderSpawnInnerRadius);
    EXPECT_FLOAT_EQ(104.0F, config.contenderSpawnOuterRadius);
    EXPECT_NO_THROW(ValidateMatchConfig(config));
}

TEST(MatchConfig, RejectsNonCanonicalTickContracts)
{
    MatchConfig config = DefaultMatchConfig();
    config.tickRate = 60U;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.botDecisionIntervalTicks = 5U;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.suddenDeathTick = 14399U;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.hardTimeoutTick = 17999U;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);
}

TEST(MatchConfig, RejectsInvalidPopulationAndSpawnAttempts)
{
    MatchConfig config = DefaultMatchConfig();
    config.contenderCount = 1U;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.maximumSpawnAttempts = 0U;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);
}

TEST(MatchConfig, AllowsSmallerFocusedPopulations)
{
    MatchConfig config = DefaultMatchConfig();
    config.contenderCount = 2U;
    config.meleeNeutralCount = 0U;
    config.rangedNeutralCount = 0U;
    config.rifleLootCount = 0U;
    config.arcPulseLootCount = 0U;
    config.medKitLootCount = 0U;
    config.contenderSpawnInnerRadius = 0.0F;
    config.contenderSpawnOuterRadius = 1.0F;

    EXPECT_NO_THROW(ValidateMatchConfig(config));
}

TEST(MatchConfig, RejectsCountsThatExceedNumericIdSpace)
{
    MatchConfig config = DefaultMatchConfig();
    config.contenderCount = std::numeric_limits<std::uint32_t>::max();
    config.meleeNeutralCount = std::numeric_limits<std::uint32_t>::max();
    config.rangedNeutralCount = std::numeric_limits<std::uint32_t>::max();
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.rifleLootCount = std::numeric_limits<std::uint32_t>::max();
    config.arcPulseLootCount = std::numeric_limits<std::uint32_t>::max();
    config.medKitLootCount = std::numeric_limits<std::uint32_t>::max();
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);
}

TEST(MatchConfig, RejectsNonFiniteAndNonPositiveMovementValues)
{
    MatchConfig config = DefaultMatchConfig();
    config.contenderSpeed = 0.0F;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.neutralSpeed = -1.0F;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.contenderPerceptionRadius =
        std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.neutralPerceptionRadius = 0.0F;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.pickupRadius = std::numeric_limits<float>::infinity();
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);
}

TEST(MatchConfig, RejectsInvalidSpacingAndSpawnRadii)
{
    MatchConfig config = DefaultMatchConfig();
    config.contenderSpawnSpacing = 0.0F;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.neutralSpawnSpacing = -1.0F;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.contenderSpawnInnerRadius = -0.1F;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.contenderSpawnOuterRadius = 19.0F;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.contenderSpawnOuterRadius =
        std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.arenaHalfExtent = 0.0F;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);

    config = DefaultMatchConfig();
    config.contenderSpawnOuterRadius = config.arenaHalfExtent + 1.0F;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);
}

TEST(MatchTypes, SnapshotsCompareEveryGameplayField)
{
    MatchSnapshot first;
    first.phase = MatchPhase::Running;
    first.actors.push_back(ActorSnapshot{
        7U,
        dxa::simulation::ActorRole::Neutral,
        NeutralArchetype::Ranged,
        {3.0F, -2.0F},
        45,
        true,
        WeaponType::Rifle,
        4U,
        1U});

    MatchSnapshot repeated = first;
    EXPECT_EQ(first, repeated);

    repeated.actors.front().health = 44;
    EXPECT_NE(first, repeated);
}

TEST(MatchTypes, OptionalIdsDistinguishAbsenceFromActorZero)
{
    MatchSnapshot waiting;
    MatchSnapshot finished = waiting;
    finished.result = MatchResult{0U, MatchEndReason::LastSurvivor, 14400U};

    EXPECT_FALSE(waiting.result.has_value());
    ASSERT_TRUE(finished.result.has_value());
    EXPECT_EQ(0U, finished.result->winner);
    EXPECT_NE(waiting, finished);

    MatchEvent zoneDeath;
    zoneDeath.type = MatchEventType::ActorDied;
    zoneDeath.actor = 5U;
    MatchEvent killedByActorZero = zoneDeath;
    killedByActorZero.subject = 0U;

    EXPECT_FALSE(zoneDeath.subject.has_value());
    ASSERT_TRUE(killedByActorZero.subject.has_value());
    EXPECT_EQ(0U, *killedByActorZero.subject);
    EXPECT_NE(zoneDeath, killedByActorZero);
}
} // namespace
