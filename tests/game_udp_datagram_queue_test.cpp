#include <dxa/game_common/UdpDatagramQueue.hpp>

#include <gtest/gtest.h>

#include <boost/asio/io_context.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace
{
using namespace std::chrono_literals;
using namespace dxa::game_common;
using namespace dxa::protocol;

[[nodiscard]] std::shared_ptr<std::vector<std::byte>> Bytes(
    const std::uint8_t value)
{
    return std::make_shared<std::vector<std::byte>>(
        1U,
        static_cast<std::byte>(value));
}

TEST(UdpDatagramQueue, DisabledProfileDeliversImmediatelyWithoutQueue)
{
    boost::asio::io_context io;
    UdpDatagramQueue queue{
        io,
        {},
        DatagramDirection::ClientToServer};
    std::uint32_t delivered = 0U;

    EXPECT_EQ(
        UdpDatagramEnqueueResult::SentImmediately,
        queue.Enqueue(1U, Bytes(1U), [&delivered](const auto&) {
            ++delivered;
        }));
    EXPECT_EQ(1U, delivered);
    EXPECT_EQ(UdpDatagramQueueMetrics{}, queue.Metrics());
}

TEST(UdpDatagramQueue, DropDoesNotInvokeDelivery)
{
    boost::asio::io_context io;
    UdpDatagramQueue queue{
        io,
        {0ms, 0ms, 10000U, 1U},
        DatagramDirection::ServerToClient};
    std::uint32_t delivered = 0U;

    EXPECT_EQ(
        UdpDatagramEnqueueResult::Dropped,
        queue.Enqueue(1U, Bytes(1U), [&delivered](const auto&) {
            ++delivered;
        }));
    EXPECT_EQ(0U, delivered);
    EXPECT_EQ(1U, queue.Metrics().dropped);
}

TEST(UdpDatagramQueue, LimitsEachPeerToTwoHundredFiftySixDelayedDatagrams)
{
    boost::asio::io_context io;
    UdpDatagramQueue queue{
        io,
        {5000ms, 0ms, 0U, 1U},
        DatagramDirection::ClientToServer};
    std::uint32_t delivered = 0U;
    for (std::uint32_t index = 0U; index < 256U; ++index)
    {
        EXPECT_EQ(
            UdpDatagramEnqueueResult::Queued,
            queue.Enqueue(7U, Bytes(1U), [&delivered](const auto&) {
                ++delivered;
            }));
    }
    EXPECT_EQ(
        UdpDatagramEnqueueResult::Overflow,
        queue.Enqueue(7U, Bytes(2U), [&delivered](const auto&) {
            ++delivered;
        }));

    EXPECT_EQ(256U, queue.Metrics().delayed);
    EXPECT_EQ(1U, queue.Metrics().overflows);
    queue.Stop();
    io.poll();
    EXPECT_EQ(0U, delivered);
}

TEST(UdpDatagramQueue, DeliversSamePeerInOrdinalOrder)
{
    boost::asio::io_context io;
    UdpDatagramQueue queue{
        io,
        {1ms, 0ms, 0U, 1U},
        DatagramDirection::ServerToClient};
    std::vector<std::uint8_t> delivered;
    constexpr std::array<std::uint8_t, 3U> values{1U, 2U, 3U};
    for (const std::uint8_t value : values)
    {
        ASSERT_EQ(
            UdpDatagramEnqueueResult::Queued,
            queue.Enqueue(9U, Bytes(value), [&delivered](const auto& bytes) {
                delivered.push_back(std::to_integer<std::uint8_t>(
                    bytes->front()));
            }));
    }

    io.run_for(50ms);
    EXPECT_EQ((std::vector<std::uint8_t>{1U, 2U, 3U}), delivered);
    EXPECT_EQ(3U, queue.Metrics().delivered);
}

TEST(UdpDatagramQueue, MoveAssignmentCancelsPreviousDelayedDatagrams)
{
    boost::asio::io_context io;
    UdpDatagramQueue queue{
        io,
        {1ms, 0ms, 0U, 1U},
        DatagramDirection::ServerToClient};
    std::uint32_t previousDelivered = 0U;
    ASSERT_EQ(
        UdpDatagramEnqueueResult::Queued,
        queue.Enqueue(1U, Bytes(1U), [&previousDelivered](const auto&) {
            ++previousDelivered;
        }));

    UdpDatagramQueue replacement{
        io,
        {},
        DatagramDirection::ServerToClient};
    queue = std::move(replacement);
    io.run_for(50ms);

    EXPECT_EQ(0U, previousDelivered);
}
} // namespace
