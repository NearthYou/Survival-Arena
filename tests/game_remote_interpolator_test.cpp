#include <dxa/game_client/RemoteInterpolator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace
{
using dxa::game_client::ReassembledSnapshot;
using dxa::game_client::RemoteInterpolator;
using dxa::protocol::EntityId;
using dxa::protocol::GameSnapshot;
using dxa::protocol::NetworkActorRole;
using dxa::protocol::NetworkActorSnapshot;
using dxa::protocol::NetworkNeutralArchetype;
using dxa::protocol::NetworkVec2;
using dxa::protocol::NetworkWeaponType;

[[nodiscard]] ReassembledSnapshot SnapshotAt(
    const std::uint32_t tick,
    const EntityId actor,
    const NetworkVec2 position,
    const std::int32_t health = 100,
    const NetworkWeaponType weapon = NetworkWeaponType::Blade)
{
    GameSnapshot snapshot;
    snapshot.aliveContenders = 1U;
    snapshot.actors.push_back(NetworkActorSnapshot{
        actor,
        NetworkActorRole::Contender,
        NetworkNeutralArchetype::None,
        position,
        health,
        health > 0,
        weapon,
        0U,
        0U});
    return ReassembledSnapshot{tick, tick, 0U, std::move(snapshot)};
}

[[nodiscard]] const NetworkActorSnapshot& FindActor(
    const GameSnapshot& snapshot,
    const EntityId actor)
{
    const auto found = std::find_if(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        [actor](const auto& candidate) { return candidate.id == actor; });
    if (found == snapshot.actors.end())
    {
        throw std::logic_error{"remote actor is absent"};
    }
    return *found;
}
} // namespace

TEST(RemoteInterpolator, SamplesThreeTicksBehindAndNeverExtrapolates)
{
    RemoteInterpolator interpolation{3U, 32U};
    interpolation.Push(SnapshotAt(10U, EntityId{4U}, {0.0F, 0.0F}));
    interpolation.Push(SnapshotAt(14U, EntityId{4U}, {4.0F, 0.0F}));
    EXPECT_EQ(
        (NetworkVec2{1.0F, 0.0F}),
        FindActor(interpolation.Sample(), EntityId{4U}).position);

    interpolation.Push(SnapshotAt(18U, EntityId{4U}, {8.0F, 0.0F}));
    EXPECT_LE(
        FindActor(interpolation.Sample(), EntityId{4U}).position.x,
        8.0F);
}

TEST(RemoteInterpolator, UsesDiscreteStateFromNewerBracket)
{
    RemoteInterpolator interpolation{3U, 32U};
    interpolation.Push(SnapshotAt(
        10U, EntityId{4U}, {0.0F, 0.0F}, 100, NetworkWeaponType::Blade));
    interpolation.Push(SnapshotAt(
        14U, EntityId{4U}, {4.0F, 0.0F}, 40, NetworkWeaponType::Rifle));

    const GameSnapshot sampled = interpolation.Sample();
    const auto& actor = FindActor(sampled, EntityId{4U});
    EXPECT_EQ(40, actor.health);
    EXPECT_EQ(NetworkWeaponType::Rifle, actor.weapon);
    EXPECT_FLOAT_EQ(1.0F, actor.position.x);
}

TEST(RemoteInterpolator, DropsLateAndDuplicateCompleteSnapshots)
{
    RemoteInterpolator interpolation{3U, 32U};
    interpolation.Push(SnapshotAt(10U, EntityId{4U}, {0.0F, 0.0F}));
    interpolation.Push(SnapshotAt(14U, EntityId{4U}, {4.0F, 0.0F}));
    interpolation.Push(SnapshotAt(12U, EntityId{4U}, {100.0F, 0.0F}));
    interpolation.Push(SnapshotAt(14U, EntityId{4U}, {100.0F, 0.0F}));

    EXPECT_FLOAT_EQ(
        1.0F,
        FindActor(interpolation.Sample(), EntityId{4U}).position.x);
}

TEST(RemoteInterpolator, ExcludesLocalActorFromSample)
{
    RemoteInterpolator interpolation{3U, 32U};
    ReassembledSnapshot first = SnapshotAt(
        10U, EntityId{1U}, {0.0F, 0.0F});
    first.snapshot.actors.push_back(NetworkActorSnapshot{
        EntityId{4U},
        NetworkActorRole::Contender,
        NetworkNeutralArchetype::None,
        {2.0F, 0.0F},
        100,
        true,
        NetworkWeaponType::Blade,
        0U,
        0U});
    first.snapshot.aliveContenders = 2U;
    interpolation.Push(std::move(first));

    const GameSnapshot sampled = interpolation.Sample(EntityId{1U});
    EXPECT_EQ(1U, sampled.actors.size());
    EXPECT_EQ(EntityId{4U}, sampled.actors.front().id);
}

TEST(RemoteInterpolator, HoldsEarliestAndLatestWhenBracketIsUnavailable)
{
    RemoteInterpolator delayed{20U, 32U};
    delayed.Push(SnapshotAt(10U, EntityId{4U}, {1.0F, 0.0F}));
    delayed.Push(SnapshotAt(14U, EntityId{4U}, {4.0F, 0.0F}));
    EXPECT_FLOAT_EQ(
        1.0F,
        FindActor(delayed.Sample(), EntityId{4U}).position.x);

    RemoteInterpolator noDelay{0U, 32U};
    noDelay.Push(SnapshotAt(10U, EntityId{4U}, {1.0F, 0.0F}));
    noDelay.Push(SnapshotAt(14U, EntityId{4U}, {4.0F, 0.0F}));
    EXPECT_FLOAT_EQ(
        4.0F,
        FindActor(noDelay.Sample(), EntityId{4U}).position.x);
}

TEST(RemoteInterpolator, KeepsOnlyThirtyTwoNewestSnapshots)
{
    RemoteInterpolator interpolation{3U, 32U};
    for (std::uint32_t tick = 1U; tick <= 40U; ++tick)
    {
        interpolation.Push(SnapshotAt(
            tick,
            EntityId{4U},
            {static_cast<float>(tick), 0.0F}));
    }

    EXPECT_EQ(32U, interpolation.BufferSize());
    EXPECT_FLOAT_EQ(
        37.0F,
        FindActor(interpolation.Sample(), EntityId{4U}).position.x);
}

TEST(RemoteInterpolator, RejectsInvalidCapacityAndEmptySample)
{
    EXPECT_THROW((void)RemoteInterpolator(3U, 0U), std::invalid_argument);
    EXPECT_THROW((void)RemoteInterpolator(3U, 33U), std::invalid_argument);
    RemoteInterpolator interpolation;
    EXPECT_THROW((void)interpolation.Sample(), std::logic_error);
}
