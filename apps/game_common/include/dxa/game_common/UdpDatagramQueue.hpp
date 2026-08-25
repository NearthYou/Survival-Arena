#pragma once

#include <dxa/protocol/DatagramShaper.hpp>

#include <boost/asio/io_context.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace dxa::game_common
{
enum class UdpDatagramEnqueueResult
{
    SentImmediately,
    Queued,
    Dropped,
    Overflow
};

struct UdpDatagramQueueMetrics
{
    std::uint64_t dropped = 0U;
    std::uint64_t delayed = 0U;
    std::uint64_t delivered = 0U;
    std::uint64_t overflows = 0U;

    [[nodiscard]] bool operator==(
        const UdpDatagramQueueMetrics&) const = default;
};

class UdpDatagramQueue
{
public:
    using Bytes = std::shared_ptr<std::vector<std::byte>>;
    using Delivery = std::function<void(const Bytes&)>;

    UdpDatagramQueue(
        boost::asio::io_context& io,
        dxa::protocol::DatagramShaperConfig config,
        dxa::protocol::DatagramDirection direction,
        std::size_t maximumQueuedPerPeer = 256U);
    ~UdpDatagramQueue();

    UdpDatagramQueue(const UdpDatagramQueue&) = delete;
    UdpDatagramQueue& operator=(const UdpDatagramQueue&) = delete;
    UdpDatagramQueue(UdpDatagramQueue&&) noexcept;
    UdpDatagramQueue& operator=(UdpDatagramQueue&&) noexcept;

    [[nodiscard]] UdpDatagramEnqueueResult Enqueue(
        std::uint64_t peerKey,
        Bytes bytes,
        Delivery delivery);
    void Stop() noexcept;
    [[nodiscard]] UdpDatagramQueueMetrics Metrics() const noexcept;

private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace dxa::game_common
