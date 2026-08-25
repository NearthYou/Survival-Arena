#pragma once

#include <dxa/protocol/AsioFramedConnection.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace dxa::game_common
{
struct GameTrafficTotals
{
    std::uint64_t tcpSentBytes = 0U;
    std::uint64_t tcpReceivedBytes = 0U;
    std::uint64_t udpSentBytes = 0U;
    std::uint64_t udpReceivedBytes = 0U;

    [[nodiscard]] bool operator==(const GameTrafficTotals&) const = default;
};

struct GameSessionMetrics
{
    GameTrafficTotals traffic;
    std::uint64_t snapshotsApplied = 0U;
    std::uint64_t snapshotsDiscarded = 0U;
    std::uint64_t snapshotQueueDrops = 0U;
    std::uint64_t keyframeRequests = 0U;

    [[nodiscard]] bool operator==(const GameSessionMetrics&) const = default;
};

class GameTrafficCounter
{
public:
    void Start(GameTrafficTotals initial = {});
    void Reset();
    void Freeze();
    void RecordTcp(dxa::protocol::TrafficDirection direction, std::size_t bytes);
    void RecordUdp(dxa::protocol::TrafficDirection direction, std::size_t bytes);
    [[nodiscard]] GameTrafficTotals Totals() const;
    [[nodiscard]] bool Active() const;

private:
    std::atomic<std::uint64_t> tcpSentBytes_{0U};
    std::atomic<std::uint64_t> tcpReceivedBytes_{0U};
    std::atomic<std::uint64_t> udpSentBytes_{0U};
    std::atomic<std::uint64_t> udpReceivedBytes_{0U};
    std::atomic<bool> active_{false};
};
} // namespace dxa::game_common
