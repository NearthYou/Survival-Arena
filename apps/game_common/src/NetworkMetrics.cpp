#include <dxa/game_common/NetworkMetrics.hpp>

#include <limits>

namespace dxa::game_common
{
namespace
{
void AddBytes(
    std::atomic<std::uint64_t>& total,
    const std::size_t bytes) noexcept
{
    const std::uint64_t value = static_cast<std::uint64_t>(bytes);
    std::uint64_t current = total.load(std::memory_order_relaxed);
    while (true)
    {
        const std::uint64_t next =
            value > std::numeric_limits<std::uint64_t>::max() - current
            ? std::numeric_limits<std::uint64_t>::max()
            : current + value;
        if (total.compare_exchange_weak(
                current,
                next,
                std::memory_order_relaxed))
        {
            return;
        }
    }
}
} // namespace

void GameTrafficCounter::Start(GameTrafficTotals initial)
{
    active_.store(false, std::memory_order_release);
    tcpSentBytes_.store(initial.tcpSentBytes, std::memory_order_relaxed);
    tcpReceivedBytes_.store(
        initial.tcpReceivedBytes,
        std::memory_order_relaxed);
    udpSentBytes_.store(initial.udpSentBytes, std::memory_order_relaxed);
    udpReceivedBytes_.store(
        initial.udpReceivedBytes,
        std::memory_order_relaxed);
    active_.store(true, std::memory_order_release);
}

void GameTrafficCounter::Reset()
{
    active_.store(false, std::memory_order_release);
    tcpSentBytes_.store(0U, std::memory_order_relaxed);
    tcpReceivedBytes_.store(0U, std::memory_order_relaxed);
    udpSentBytes_.store(0U, std::memory_order_relaxed);
    udpReceivedBytes_.store(0U, std::memory_order_relaxed);
}

void GameTrafficCounter::Freeze()
{
    active_.store(false, std::memory_order_release);
}

void GameTrafficCounter::RecordTcp(
    const dxa::protocol::TrafficDirection direction,
    const std::size_t bytes)
{
    if (!active_.load(std::memory_order_acquire))
    {
        return;
    }
    AddBytes(
        direction == dxa::protocol::TrafficDirection::Sent
            ? tcpSentBytes_
            : tcpReceivedBytes_,
        bytes);
}

void GameTrafficCounter::RecordUdp(
    const dxa::protocol::TrafficDirection direction,
    const std::size_t bytes)
{
    if (!active_.load(std::memory_order_acquire))
    {
        return;
    }
    AddBytes(
        direction == dxa::protocol::TrafficDirection::Sent
            ? udpSentBytes_
            : udpReceivedBytes_,
        bytes);
}

GameTrafficTotals GameTrafficCounter::Totals() const
{
    return {
        tcpSentBytes_.load(std::memory_order_relaxed),
        tcpReceivedBytes_.load(std::memory_order_relaxed),
        udpSentBytes_.load(std::memory_order_relaxed),
        udpReceivedBytes_.load(std::memory_order_relaxed)};
}

bool GameTrafficCounter::Active() const
{
    return active_.load(std::memory_order_acquire);
}
} // namespace dxa::game_common
