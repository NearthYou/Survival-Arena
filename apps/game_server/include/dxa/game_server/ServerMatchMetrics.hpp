#pragma once

#include <dxa/game_common/NetworkMetrics.hpp>
#include <dxa/protocol/Ids.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dxa::game_server
{
struct ReplicationMetricSample
{
    std::chrono::nanoseconds encodeDuration{};
    std::uint32_t payloadBytes = 0U;
    std::uint16_t fragmentCount = 0U;
    std::uint16_t visibleActors = 0U;
    std::uint16_t visibleLoot = 0U;
    bool keyframe = false;
    bool fallbackKeyframe = false;

    [[nodiscard]] bool operator==(const ReplicationMetricSample&) const = default;
};

struct ServerMatchMetricsSnapshot
{
    dxa::protocol::MatchId match;
    std::vector<std::chrono::nanoseconds> tickSamples;
    std::vector<ReplicationMetricSample> replicationSamples;
    std::chrono::nanoseconds tickP95{};
    std::chrono::nanoseconds replicationP95{};
    std::uint64_t tcpBytes = 0U;
    std::uint64_t udpBytes = 0U;
    std::uint64_t payloadBytes = 0U;
    std::uint64_t schedulerOverruns = 0U;

    [[nodiscard]] bool operator==(
        const ServerMatchMetricsSnapshot&) const = default;
};

class ServerMatchMetrics
{
public:
    explicit ServerMatchMetrics(std::size_t maximumSamples);
    ServerMatchMetrics(
        dxa::protocol::MatchId match,
        std::size_t maximumTickSamples,
        std::size_t maximumReplicationSamples);

    void RecordTick(std::chrono::nanoseconds duration);
    void RecordReplication(
        std::chrono::nanoseconds encodeDuration,
        std::uint32_t payloadBytes,
        std::uint16_t fragmentCount,
        bool keyframe,
        std::uint16_t visibleActors,
        std::uint16_t visibleLoot,
        bool fallbackKeyframe = false);
    [[nodiscard]] ServerMatchMetricsSnapshot Snapshot(
        dxa::game_common::GameTrafficTotals traffic = {},
        std::uint64_t schedulerOverruns = 0U) const;

private:
    dxa::protocol::MatchId match_;
    std::size_t maximumTickSamples_ = 0U;
    std::size_t maximumReplicationSamples_ = 0U;
    std::vector<std::chrono::nanoseconds> tickSamples_;
    std::vector<ReplicationMetricSample> replicationSamples_;
    std::uint64_t payloadBytes_ = 0U;
};
} // namespace dxa::game_server
