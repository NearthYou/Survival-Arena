#include <dxa/simulation/Combat.hpp>

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{
using dxa::simulation::ActorId;
using dxa::simulation::ActorRole;
using dxa::simulation::AttackIntent;
using dxa::simulation::CombatActor;
using dxa::simulation::CombatResolution;
using dxa::simulation::DamageRecord;
using dxa::simulation::DeathRecord;
using dxa::simulation::ResolveAttacks;
using dxa::simulation::TickWeaponCooldowns;
using dxa::simulation::Vec2;
using dxa::simulation::WeaponDefinition;
using dxa::simulation::WeaponDefinitionFor;
using dxa::simulation::WeaponType;

[[nodiscard]] CombatActor Contender(
    const ActorId id,
    const Vec2 position,
    const WeaponType weapon = WeaponType::Blade,
    const int health = 100)
{
    return CombatActor{id, ActorRole::Contender, position, health, true, weapon};
}

[[nodiscard]] CombatActor Neutral(
    const ActorId id,
    const Vec2 position,
    const WeaponType weapon = WeaponType::Blade,
    const int health = 60)
{
    return CombatActor{id, ActorRole::Neutral, position, health, true, weapon};
}

[[nodiscard]] const DamageRecord& DamageTo(
    const CombatResolution& resolution,
    const ActorId target)
{
    for (const DamageRecord& damage : resolution.damage)
    {
        if (damage.target == target)
        {
            return damage;
        }
    }
    throw std::logic_error{"expected damage record is missing"};
}

[[nodiscard]] const DeathRecord& DeathOf(
    const CombatResolution& resolution,
    const ActorId victim)
{
    for (const DeathRecord& death : resolution.deaths)
    {
        if (death.victim == victim)
        {
            return death;
        }
    }
    throw std::logic_error{"expected death record is missing"};
}

TEST(Combat, LocksWeaponDefinitions)
{
    EXPECT_EQ(
        (WeaponDefinition{2.2F, 0.0F, 24, 21U}),
        WeaponDefinitionFor(WeaponType::Blade));
    EXPECT_EQ(
        (WeaponDefinition{18.0F, 0.0F, 12, 12U}),
        WeaponDefinitionFor(WeaponType::Rifle));
    EXPECT_EQ(
        (WeaponDefinition{10.0F, 5.0F, 18, 90U}),
        WeaponDefinitionFor(WeaponType::ArcPulse));
}

TEST(Combat, RejectsUnknownWeaponDefinition)
{
    EXPECT_THROW(
        (void)WeaponDefinitionFor(static_cast<WeaponType>(99)),
        std::invalid_argument);
}

TEST(Combat, AppliesInclusiveRangeAndStartsCooldown)
{
    std::vector actors{
        Contender(0U, {0.0F, 0.0F}),
        Contender(1U, {2.2F, 0.0F})};

    const CombatResolution resolution = ResolveAttacks(
        actors,
        std::array{AttackIntent{0U, 1U}});

    EXPECT_EQ((std::vector{AttackIntent{0U, 1U}}), resolution.acceptedIntents);
    EXPECT_EQ(76, actors[1].health);
    EXPECT_EQ(21U, actors[0].cooldownTicksRemaining);
    EXPECT_EQ(24, DamageTo(resolution, 1U).amount);
    ASSERT_TRUE(DamageTo(resolution, 1U).primarySource.has_value());
    EXPECT_EQ(0U, *DamageTo(resolution, 1U).primarySource);
    EXPECT_TRUE(resolution.deaths.empty());
}

TEST(Combat, RejectsOutOfRangeAndCooldownBlockedAttacksWithoutMutation)
{
    std::vector outOfRange{
        Contender(0U, {0.0F, 0.0F}),
        Contender(1U, {2.21F, 0.0F})};
    const std::vector originalOutOfRange = outOfRange;
    EXPECT_TRUE(ResolveAttacks(
                    outOfRange,
                    std::array{AttackIntent{0U, 1U}})
                    .acceptedIntents.empty());
    EXPECT_EQ(originalOutOfRange, outOfRange);

    std::vector cooldownBlocked{
        Contender(0U, {0.0F, 0.0F}),
        Contender(1U, {1.0F, 0.0F})};
    cooldownBlocked[0].cooldownTicksRemaining = 1U;
    const std::vector originalCooldownBlocked = cooldownBlocked;
    EXPECT_TRUE(ResolveAttacks(
                    cooldownBlocked,
                    std::array{AttackIntent{0U, 1U}})
                    .acceptedIntents.empty());
    EXPECT_EQ(originalCooldownBlocked, cooldownBlocked);
}

TEST(Combat, DecrementsPositiveCooldownsWithoutUnderflow)
{
    std::vector actors{
        Contender(0U, {0.0F, 0.0F}),
        Contender(1U, {1.0F, 0.0F})};
    actors[0].cooldownTicksRemaining = 2U;
    actors[1].cooldownTicksRemaining = 0U;

    TickWeaponCooldowns(actors);

    EXPECT_EQ(1U, actors[0].cooldownTicksRemaining);
    EXPECT_EQ(0U, actors[1].cooldownTicksRemaining);
}

TEST(Combat, RejectsSelfTarget)
{
    std::vector actors{
        Contender(0U, {0.0F, 0.0F}),
        Contender(1U, {1.0F, 0.0F})};

    const CombatResolution resolution = ResolveAttacks(
        actors,
        std::array{AttackIntent{0U, 0U}});

    EXPECT_TRUE(resolution.acceptedIntents.empty());
    EXPECT_TRUE(resolution.damage.empty());
    EXPECT_TRUE(resolution.deaths.empty());
}

TEST(Combat, RejectsDeadTarget)
{
    std::vector actors{
        Contender(0U, {0.0F, 0.0F}),
        Contender(1U, {1.0F, 0.0F})};
    actors[1].alive = false;
    actors[1].health = 0;

    const CombatResolution resolution = ResolveAttacks(
        actors,
        std::array{AttackIntent{0U, 1U}});

    EXPECT_TRUE(resolution.acceptedIntents.empty());
    EXPECT_TRUE(resolution.damage.empty());
    EXPECT_TRUE(resolution.deaths.empty());
}

TEST(Combat, RejectsMissingAttackerAndTarget)
{
    std::vector actors{
        Contender(0U, {0.0F, 0.0F}),
        Contender(1U, {1.0F, 0.0F})};

    const CombatResolution missingTarget = ResolveAttacks(
        actors,
        std::array{AttackIntent{0U, 99U}});
    const CombatResolution missingAttacker = ResolveAttacks(
        actors,
        std::array{AttackIntent{99U, 0U}});

    EXPECT_TRUE(missingTarget.acceptedIntents.empty());
    EXPECT_TRUE(missingTarget.damage.empty());
    EXPECT_TRUE(missingAttacker.acceptedIntents.empty());
    EXPECT_TRUE(missingAttacker.damage.empty());
}

TEST(Combat, NeutralTargetsOnlyContenders)
{
    std::vector actors{
        Neutral(10U, {0.0F, 0.0F}),
        Neutral(11U, {1.0F, 0.0F}),
        Contender(2U, {1.5F, 0.0F})};

    const CombatResolution neutralTarget = ResolveAttacks(
        actors,
        std::array{AttackIntent{10U, 11U}});
    EXPECT_TRUE(neutralTarget.acceptedIntents.empty());

    const CombatResolution contenderTarget = ResolveAttacks(
        actors,
        std::array{AttackIntent{10U, 2U}});
    EXPECT_EQ((std::vector{AttackIntent{10U, 2U}}), contenderTarget.acceptedIntents);
    EXPECT_EQ(76, actors[2].health);
}

TEST(Combat, ContenderCanDamageNeutral)
{
    std::vector actors{
        Contender(0U, {0.0F, 0.0F}),
        Neutral(10U, {1.0F, 0.0F})};

    const CombatResolution resolution = ResolveAttacks(
        actors,
        std::array{AttackIntent{0U, 10U}});

    EXPECT_EQ(36, actors[1].health);
    EXPECT_EQ(24, DamageTo(resolution, 10U).amount);
}

TEST(Combat, ArcPulseDamagesActorsAroundTargetButNeverItsAttacker)
{
    std::vector actors{
        Contender(0U, {0.0F, 0.0F}, WeaponType::ArcPulse),
        Contender(1U, {2.0F, 0.0F}),
        Contender(2U, {6.9F, 0.0F}),
        Contender(3U, {7.1F, 0.0F})};

    const CombatResolution resolution = ResolveAttacks(
        actors,
        std::array{AttackIntent{0U, 1U}});

    EXPECT_EQ(100, actors[0].health);
    EXPECT_EQ(82, actors[1].health);
    EXPECT_EQ(82, actors[2].health);
    EXPECT_EQ(100, actors[3].health);
    EXPECT_EQ(2U, resolution.damage.size());
    EXPECT_EQ(90U, actors[0].cooldownTicksRemaining);
}

TEST(Combat, ResolvesSameTickDamageWithoutIntentOrderBias)
{
    std::vector forward{
        Contender(0U, {0.0F, 0.0F}),
        Contender(1U, {0.5F, 0.0F}),
        Contender(2U, {1.5F, 0.0F}, WeaponType::Blade, 30)};
    std::vector reverse = forward;
    const std::array forwardIntents{AttackIntent{0U, 2U}, AttackIntent{1U, 2U}};
    const std::array reverseIntents{AttackIntent{1U, 2U}, AttackIntent{0U, 2U}};

    const CombatResolution forwardResult = ResolveAttacks(forward, forwardIntents);
    const CombatResolution reverseResult = ResolveAttacks(reverse, reverseIntents);

    EXPECT_EQ(forwardResult, reverseResult);
    EXPECT_EQ(forward, reverse);
    EXPECT_EQ(0, forward[2].health);
    EXPECT_FALSE(forward[2].alive);
    ASSERT_TRUE(DeathOf(forwardResult, 2U).killer.has_value());
    EXPECT_EQ(0U, *DeathOf(forwardResult, 2U).killer);
}

TEST(Combat, AttackerKilledInSameTickStillAppliesValidatedAttack)
{
    std::vector actors{
        Contender(0U, {0.0F, 0.0F}, WeaponType::Blade, 24),
        Contender(1U, {1.0F, 0.0F}, WeaponType::Blade, 24)};

    const CombatResolution resolution = ResolveAttacks(
        actors,
        std::array{AttackIntent{0U, 1U}, AttackIntent{1U, 0U}});

    EXPECT_FALSE(actors[0].alive);
    EXPECT_FALSE(actors[1].alive);
    EXPECT_EQ(2U, resolution.deaths.size());
    EXPECT_EQ(1U, actors[0].eliminations);
    EXPECT_EQ(1U, actors[1].eliminations);
}

TEST(Combat, CreditsHighestDamageContributorForLethalBatch)
{
    std::vector actors{
        Contender(0U, {0.0F, 0.0F}, WeaponType::Rifle),
        Contender(1U, {0.5F, 0.0F}, WeaponType::Blade),
        Contender(2U, {1.5F, 0.0F}, WeaponType::Blade, 30)};

    const CombatResolution resolution = ResolveAttacks(
        actors,
        std::array{AttackIntent{0U, 2U}, AttackIntent{1U, 2U}});

    ASSERT_TRUE(DeathOf(resolution, 2U).killer.has_value());
    EXPECT_EQ(1U, *DeathOf(resolution, 2U).killer);
    EXPECT_EQ(0U, actors[0].eliminations);
    EXPECT_EQ(1U, actors[1].eliminations);
}

TEST(Combat, CreditsLowerActorIdWhenDamageContributionsTie)
{
    std::vector actors{
        Contender(8U, {0.0F, 0.0F}),
        Contender(3U, {0.5F, 0.0F}),
        Contender(20U, {1.5F, 0.0F}, WeaponType::Blade, 30)};

    const CombatResolution resolution = ResolveAttacks(
        actors,
        std::array{AttackIntent{8U, 20U}, AttackIntent{3U, 20U}});

    ASSERT_TRUE(DeathOf(resolution, 20U).killer.has_value());
    EXPECT_EQ(3U, *DeathOf(resolution, 20U).killer);
}

TEST(Combat, ArcPulseCreditsEveryLethalTargetOnce)
{
    std::vector actors{
        Contender(0U, {0.0F, 0.0F}, WeaponType::ArcPulse),
        Contender(1U, {2.0F, 0.0F}, WeaponType::Blade, 18),
        Contender(2U, {4.0F, 0.0F}, WeaponType::Blade, 10)};

    const CombatResolution resolution = ResolveAttacks(
        actors,
        std::array{AttackIntent{0U, 1U}});

    EXPECT_EQ(2U, resolution.deaths.size());
    EXPECT_EQ(2U, actors[0].eliminations);
    EXPECT_EQ(18, DamageTo(resolution, 1U).amount);
    EXPECT_EQ(10, DamageTo(resolution, 2U).amount);
}

TEST(Combat, RejectsInvalidActorStateAndDuplicateIds)
{
    std::vector duplicateIds{
        Contender(0U, {0.0F, 0.0F}),
        Contender(0U, {1.0F, 0.0F})};
    EXPECT_THROW(
        (void)ResolveAttacks(duplicateIds, std::span<const AttackIntent>{}),
        std::invalid_argument);

    std::vector nonFinite{
        Contender(0U, {std::numeric_limits<float>::infinity(), 0.0F})};
    EXPECT_THROW(
        (void)ResolveAttacks(nonFinite, std::span<const AttackIntent>{}),
        std::invalid_argument);

    std::vector inconsistent{Contender(0U, {0.0F, 0.0F}, WeaponType::Blade, 0)};
    EXPECT_THROW(
        (void)ResolveAttacks(inconsistent, std::span<const AttackIntent>{}),
        std::invalid_argument);
}

TEST(Combat, RejectsMultipleIntentsFromOneAttacker)
{
    std::vector actors{
        Contender(0U, {0.0F, 0.0F}),
        Contender(1U, {1.0F, 0.0F}),
        Contender(2U, {2.0F, 0.0F})};

    EXPECT_THROW(
        (void)ResolveAttacks(
            actors,
            std::array{AttackIntent{0U, 1U}, AttackIntent{0U, 2U}}),
        std::invalid_argument);
}
} // namespace
