#include <dxa/game_client/SnapshotReassembler.hpp>

#include <dxa/protocol/GameSnapshotCodec.hpp>
#include <dxa/protocol/GameUdpCodec.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
using dxa::game_client::ReassembledSnapshot;
using dxa::game_client::ReassembledPayload;
using dxa::game_client::SnapshotReassembler;
using dxa::protocol::EntityId;
using dxa::protocol::GameSnapshot;
using dxa::protocol::MatchId;
using dxa::protocol::NetworkActorRole;
using dxa::protocol::NetworkActorSnapshot;
using dxa::protocol::NetworkNeutralArchetype;
using dxa::protocol::NetworkWeaponType;
using dxa::protocol::SnapshotFragment;

[[nodiscard]] GameSnapshot Snapshot(const std::uint32_t actorCount)
{
    GameSnapshot snapshot;
    snapshot.aliveContenders = actorCount;
    snapshot.actors.reserve(actorCount);
    for (std::uint32_t actor = 0U; actor < actorCount; ++actor)
    {
        snapshot.actors.push_back(NetworkActorSnapshot{
            EntityId{actor},
            NetworkActorRole::Contender,
            NetworkNeutralArchetype::None,
            {static_cast<float>(actor), 0.0F},
            100,
            true,
            NetworkWeaponType::Blade,
            0U,
            0U});
    }
    return snapshot;
}

[[nodiscard]] std::vector<SnapshotFragment> MakeFragments(
    const std::uint32_t snapshotId,
    const std::uint32_t serverTick,
    const std::uint32_t ack,
    const std::uint32_t actorCount = 50U)
{
    const std::vector<std::byte> payload =
        dxa::protocol::EncodeGameSnapshot(Snapshot(actorCount));
    return dxa::protocol::FragmentSnapshot(
        MatchId{7U},
        snapshotId,
        serverTick,
        ack,
        payload);
}
} // namespace

TEST(SnapshotReassembler, NewerSnapshotDiscardsIncompleteOlderSnapshot)
{
    SnapshotReassembler reassembler;
    const auto older = MakeFragments(10U, 10U, 1U);
    const auto newer = MakeFragments(11U, 11U, 2U);
    ASSERT_GT(older.size(), 1U);
    EXPECT_FALSE(reassembler.Push(older.front()).has_value());

    std::optional<ReassembledSnapshot> completed;
    for (const SnapshotFragment& fragment : newer)
    {
        completed = reassembler.Push(fragment);
    }

    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(11U, completed->snapshotId);
    EXPECT_EQ(11U, completed->serverTick);
    EXPECT_EQ(2U, completed->ackInputSequence);
}

TEST(SnapshotReassembler, AcceptsOutOfOrderFragmentsAndIgnoresExactDuplicate)
{
    SnapshotReassembler reassembler;
    auto fragments = MakeFragments(20U, 30U, 4U);
    ASSERT_GT(fragments.size(), 1U);

    EXPECT_FALSE(reassembler.Push(fragments.back()).has_value());
    EXPECT_FALSE(reassembler.Push(fragments.back()).has_value());
    std::optional<ReassembledSnapshot> completed;
    for (std::size_t index = 0U; index + 1U < fragments.size(); ++index)
    {
        completed = reassembler.Push(fragments[index]);
    }

    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(50U, completed->snapshot.actors.size());
}

TEST(SnapshotReassembler, MetadataMismatchPoisonsCurrentSnapshot)
{
    SnapshotReassembler reassembler;
    auto fragments = MakeFragments(21U, 31U, 5U);
    ASSERT_GT(fragments.size(), 1U);
    EXPECT_FALSE(reassembler.Push(fragments[0]).has_value());
    SnapshotFragment mismatch = fragments[1];
    ++mismatch.serverTick;
    EXPECT_FALSE(reassembler.Push(mismatch).has_value());

    std::optional<ReassembledSnapshot> completed;
    for (const SnapshotFragment& fragment : fragments)
    {
        completed = reassembler.Push(fragment);
    }
    EXPECT_FALSE(completed.has_value());
}

TEST(SnapshotReassembler, RejectsCrcMismatchAndDeliveredReplay)
{
    SnapshotReassembler reassembler;
    auto corrupt = MakeFragments(22U, 32U, 6U);
    corrupt.back().bytes.back() ^= std::byte{0x01};
    std::optional<ReassembledSnapshot> completed;
    for (const SnapshotFragment& fragment : corrupt)
    {
        completed = reassembler.Push(fragment);
    }
    EXPECT_FALSE(completed.has_value());

    const auto valid = MakeFragments(23U, 33U, 7U);
    for (const SnapshotFragment& fragment : valid)
    {
        completed = reassembler.Push(fragment);
    }
    ASSERT_TRUE(completed.has_value());
    completed.reset();
    for (const SnapshotFragment& fragment : valid)
    {
        completed = reassembler.Push(fragment);
    }
    EXPECT_FALSE(completed.has_value());
}

TEST(SnapshotReassembler, DropsLateCompleteSnapshotAfterNewerDelivery)
{
    SnapshotReassembler reassembler;
    const auto older = MakeFragments(30U, 40U, 8U);
    const auto newer = MakeFragments(31U, 41U, 9U);
    std::optional<ReassembledSnapshot> completed;
    for (const SnapshotFragment& fragment : newer)
    {
        completed = reassembler.Push(fragment);
    }
    ASSERT_TRUE(completed.has_value());

    completed.reset();
    for (const SnapshotFragment& fragment : older)
    {
        completed = reassembler.Push(fragment);
    }
    EXPECT_FALSE(completed.has_value());
}

TEST(SnapshotReassembler, HandlesThirtyTwoFragmentBoundaryWithoutDelivery)
{
    SnapshotReassembler reassembler;
    std::vector<std::byte> payload(dxa::protocol::MaxSnapshotPayloadBytes);
    const auto fragments = dxa::protocol::FragmentSnapshot(
        MatchId{7U},
        40U,
        50U,
        10U,
        payload);
    ASSERT_EQ(32U, fragments.size());
    for (const SnapshotFragment& fragment : fragments)
    {
        EXPECT_FALSE(reassembler.Push(fragment).has_value());
    }
}

TEST(SnapshotReassembler, ReturnsVerifiedBytesWithoutDecodingPayload)
{
    SnapshotReassembler reassembler;
    const std::vector<std::byte> payload(
        dxa::protocol::MaxSnapshotPayloadBytes,
        std::byte{0xA5});
    const std::vector<SnapshotFragment> fragments =
        dxa::protocol::FragmentSnapshot(
            MatchId{7U},
            41U,
            51U,
            11U,
            payload);

    std::optional<ReassembledPayload> completed;
    for (const SnapshotFragment& fragment : fragments)
    {
        completed = reassembler.PushBytes(fragment);
    }

    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(41U, completed->snapshotId);
    EXPECT_EQ(51U, completed->serverTick);
    EXPECT_EQ(11U, completed->ackInputSequence);
    EXPECT_EQ(payload, completed->bytes);
}

TEST(SnapshotReassembler, ResetAllowsFreshLowerIdentityStream)
{
    SnapshotReassembler reassembler;
    const auto high = MakeFragments(100U, 100U, 1U, 2U);
    std::optional<ReassembledSnapshot> completed;
    for (const auto& fragment : high)
    {
        completed = reassembler.Push(fragment);
    }
    ASSERT_TRUE(completed.has_value());

    reassembler.Reset();
    const auto low = MakeFragments(1U, 1U, 0U, 2U);
    completed.reset();
    for (const auto& fragment : low)
    {
        completed = reassembler.Push(fragment);
    }
    EXPECT_TRUE(completed.has_value());
}
