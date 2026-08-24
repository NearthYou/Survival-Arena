#include <dxa/protocol/GameTypes.hpp>
#include <dxa/protocol/Ids.hpp>
#include <dxa/protocol/LobbyTypes.hpp>

#include <gtest/gtest.h>

#include <type_traits>

namespace
{
using namespace dxa::protocol;

TEST(GameTypes, KeepsWorkerIdentifiersDistinctFromPublicDomainIds)
{
    static_assert(!std::is_same_v<WorkerId, PlayerId>);
    static_assert(!std::is_same_v<ReservationId, MatchId>);
    static_assert(!std::is_same_v<MatchTicketValue, UdpSessionToken>);

    EXPECT_LT(WorkerId{1U}, WorkerId{2U});
    EXPECT_LT(ReservationId{1U}, ReservationId{2U});
}

TEST(GameTypes, LocksNetworkEnvelopeValuesConsumedByFrameCodecs)
{
    EXPECT_EQ(13U, static_cast<unsigned>(MessageType::WorkerRegister));
    EXPECT_EQ(24U, static_cast<unsigned>(MessageType::GameMatchResult));
    EXPECT_EQ(20U, static_cast<unsigned>(LobbyError::MatchUnavailable));
    EXPECT_EQ(30U, GameTickRate);
    EXPECT_EQ(15U, SnapshotRate);
    EXPECT_EQ(10U, UdpHeaderBytes);
    EXPECT_EQ(1200U, MaxUdpDatagramBytes);
    EXPECT_EQ(32U, SnapshotFragmentMetadataBytes);
    EXPECT_EQ(1158U, MaxSnapshotFragmentPayloadBytes);
    EXPECT_EQ(32U, MaxSnapshotFragments);
    EXPECT_EQ(37056U, MaxSnapshotPayloadBytes);
    EXPECT_EQ(256U, MaxClientInputHistory);
    EXPECT_EQ(32U, MaxClientSnapshotBuffer);
}
} // namespace
