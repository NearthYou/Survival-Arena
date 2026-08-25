#include <dxa/game_common/UdpDatagramQueue.hpp>

#include <boost/asio/steady_timer.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace dxa::game_common
{
struct UdpDatagramQueue::State
    : public std::enable_shared_from_this<UdpDatagramQueue::State>
{
    using Clock = std::chrono::steady_clock;

    struct Item
    {
        Clock::time_point deliveryTime;
        std::uint64_t ordinal = 0U;
        Bytes bytes;
        Delivery delivery;
    };

    struct Peer
    {
        explicit Peer(boost::asio::io_context& io)
            : timer{io}
        {
        }

        boost::asio::steady_timer timer;
        std::uint64_t nextOrdinal = 1U;
        std::uint64_t timerGeneration = 0U;
        bool timerArmed = false;
        std::deque<Item> items;
    };

    State(
        boost::asio::io_context& sourceIo,
        dxa::protocol::DatagramShaperConfig config,
        const dxa::protocol::DatagramDirection direction,
        const std::size_t maximum)
        : io{sourceIo},
          shaper{std::move(config), direction},
          maximumQueuedPerPeer{maximum}
    {
        if (maximumQueuedPerPeer == 0U)
        {
            throw std::invalid_argument{
                "UDP datagram queue capacity must be positive"};
        }
    }

    [[nodiscard]] UdpDatagramEnqueueResult Enqueue(
        const std::uint64_t peerKey,
        Bytes bytes,
        Delivery delivery)
    {
        if (stopped.load())
        {
            return UdpDatagramEnqueueResult::Overflow;
        }
        if (!bytes || bytes->empty() || !delivery)
        {
            throw std::invalid_argument{"UDP datagram delivery is invalid"};
        }
        if (!shaper.Enabled())
        {
            delivery(bytes);
            return UdpDatagramEnqueueResult::SentImmediately;
        }

        Peer& peer = PeerFor(peerKey);
        if (peer.nextOrdinal == 0U)
        {
            ++overflows;
            return UdpDatagramEnqueueResult::Overflow;
        }
        const std::uint64_t ordinal = peer.nextOrdinal++;
        const dxa::protocol::ShapedDatagramDecision decision = shaper.Decide(
            peerKey,
            ordinal);
        if (decision.drop)
        {
            ++dropped;
            return UdpDatagramEnqueueResult::Dropped;
        }
        if (decision.delay <= std::chrono::milliseconds::zero())
        {
            delivery(bytes);
            return UdpDatagramEnqueueResult::SentImmediately;
        }
        if (peer.items.size() >= maximumQueuedPerPeer)
        {
            ++overflows;
            return UdpDatagramEnqueueResult::Overflow;
        }

        Item item{
            Clock::now() + decision.delay,
            ordinal,
            std::move(bytes),
            std::move(delivery)};
        const auto insertion = std::upper_bound(
            peer.items.begin(),
            peer.items.end(),
            item,
            [](const Item& left, const Item& right) {
                if (left.deliveryTime != right.deliveryTime)
                {
                    return left.deliveryTime < right.deliveryTime;
                }
                return left.ordinal < right.ordinal;
            });
        const bool newFront = insertion == peer.items.begin();
        peer.items.insert(insertion, std::move(item));
        ++delayed;
        if (newFront || !peer.timerArmed)
        {
            Arm(peerKey, peer);
        }
        return UdpDatagramEnqueueResult::Queued;
    }

    void Stop() noexcept
    {
        if (stopped.exchange(true))
        {
            return;
        }
        for (auto& [key, peer] : peers)
        {
            static_cast<void>(key);
            try
            {
                peer->timer.cancel();
            }
            catch (const std::exception&)
            {
            }
            peer->items.clear();
            peer->timerArmed = false;
        }
        peers.clear();
    }

    [[nodiscard]] UdpDatagramQueueMetrics Metrics() const noexcept
    {
        return {
            dropped.load(),
            delayed.load(),
            delivered.load(),
            overflows.load()};
    }

    [[nodiscard]] Peer& PeerFor(const std::uint64_t peerKey)
    {
        auto [entry, inserted] = peers.try_emplace(peerKey);
        if (inserted)
        {
            entry->second = std::make_unique<Peer>(io);
        }
        return *entry->second;
    }

    void Arm(const std::uint64_t peerKey, Peer& peer)
    {
        if (peer.items.empty() || stopped.load())
        {
            peer.timerArmed = false;
            return;
        }
        peer.timer.cancel();
        peer.timer.expires_at(peer.items.front().deliveryTime);
        peer.timerArmed = true;
        const std::uint64_t generation = ++peer.timerGeneration;
        const auto self = shared_from_this();
        peer.timer.async_wait(
            [self, peerKey, generation](
                const boost::system::error_code error) {
                const auto found = self->peers.find(peerKey);
                if (found == self->peers.end()
                    || generation != found->second->timerGeneration)
                {
                    return;
                }
                Peer& active = *found->second;
                active.timerArmed = false;
                if (error || self->stopped.load())
                {
                    return;
                }
                self->DeliverDue(peerKey, active);
            });
    }

    void DeliverDue(const std::uint64_t peerKey, Peer& peer)
    {
        const Clock::time_point now = Clock::now();
        while (!peer.items.empty()
               && peer.items.front().deliveryTime <= now)
        {
            Item item = std::move(peer.items.front());
            peer.items.pop_front();
            item.delivery(item.bytes);
            ++delivered;
        }
        Arm(peerKey, peer);
    }

    boost::asio::io_context& io;
    dxa::protocol::DatagramShaper shaper;
    std::size_t maximumQueuedPerPeer = 256U;
    std::map<std::uint64_t, std::unique_ptr<Peer>> peers;
    std::atomic<std::uint64_t> dropped{0U};
    std::atomic<std::uint64_t> delayed{0U};
    std::atomic<std::uint64_t> delivered{0U};
    std::atomic<std::uint64_t> overflows{0U};
    std::atomic<bool> stopped{false};
};

UdpDatagramQueue::UdpDatagramQueue(
    boost::asio::io_context& io,
    dxa::protocol::DatagramShaperConfig config,
    const dxa::protocol::DatagramDirection direction,
    const std::size_t maximumQueuedPerPeer)
    : state_{std::make_shared<State>(
          io,
          std::move(config),
          direction,
          maximumQueuedPerPeer)}
{
}

UdpDatagramQueue::~UdpDatagramQueue()
{
    Stop();
}

UdpDatagramQueue::UdpDatagramQueue(UdpDatagramQueue&&) noexcept = default;
UdpDatagramQueue& UdpDatagramQueue::operator=(
    UdpDatagramQueue&&) noexcept = default;

UdpDatagramEnqueueResult UdpDatagramQueue::Enqueue(
    const std::uint64_t peerKey,
    Bytes bytes,
    Delivery delivery)
{
    if (!state_)
    {
        throw std::logic_error{"UDP datagram queue was moved from"};
    }
    return state_->Enqueue(peerKey, std::move(bytes), std::move(delivery));
}

void UdpDatagramQueue::Stop() noexcept
{
    if (state_)
    {
        state_->Stop();
    }
}

UdpDatagramQueueMetrics UdpDatagramQueue::Metrics() const noexcept
{
    return state_ ? state_->Metrics() : UdpDatagramQueueMetrics{};
}
} // namespace dxa::game_common
