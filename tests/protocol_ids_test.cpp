#include <dxa/protocol/Ids.hpp>
#include <dxa/protocol/LobbyTypes.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <type_traits>

TEST(ProtocolIds, KeepsDomainIdsDistinct)
{
    static_assert(!std::is_same_v<dxa::protocol::PlayerId, dxa::protocol::RoomId>);
    static_assert(!std::is_same_v<dxa::protocol::RoomId, dxa::protocol::MatchId>);
    static_assert(!std::is_same_v<dxa::protocol::MatchId, dxa::protocol::EntityId>);
    static_assert(!std::is_same_v<dxa::protocol::WorkerId, dxa::protocol::PlayerId>);
    static_assert(!std::is_same_v<dxa::protocol::ReservationId, dxa::protocol::MatchId>);

    EXPECT_LT(dxa::protocol::PlayerId{1U}, dxa::protocol::PlayerId{2U});
    EXPECT_LT(dxa::protocol::RoomId{1U}, dxa::protocol::RoomId{2U});
    EXPECT_LT(dxa::protocol::MatchId{1U}, dxa::protocol::MatchId{2U});
    EXPECT_LT(dxa::protocol::EntityId{1U}, dxa::protocol::EntityId{2U});
    EXPECT_LT(dxa::protocol::WorkerId{1U}, dxa::protocol::WorkerId{2U});
    EXPECT_LT(dxa::protocol::ReservationId{1U}, dxa::protocol::ReservationId{2U});
}

TEST(LobbyTypes, LocksWireConstants)
{
    EXPECT_EQ(1U, dxa::protocol::ProtocolVersion);
    EXPECT_EQ(std::size_t{12}, dxa::protocol::TcpFrameHeaderBytes);
    EXPECT_EQ(std::size_t{65536}, dxa::protocol::MaxTcpFrameBytes);
    EXPECT_EQ(std::size_t{65524}, dxa::protocol::MaxTcpPayloadBytes);
    EXPECT_EQ(std::size_t{24}, dxa::protocol::RoomCapacity);
    EXPECT_EQ(std::size_t{1024}, dxa::protocol::MaximumRooms);
    EXPECT_EQ(std::size_t{16}, dxa::protocol::MatchTicketBytes);
    EXPECT_EQ(std::size_t{60}, dxa::protocol::MatchTicketLifetimeSeconds);
    EXPECT_EQ(std::size_t{262144}, dxa::protocol::MaxPendingWriteBytes);
    EXPECT_EQ(1U, static_cast<unsigned>(dxa::protocol::MessageType::ClientHello));
    EXPECT_EQ(12U, static_cast<unsigned>(dxa::protocol::MessageType::ErrorResponse));
}
