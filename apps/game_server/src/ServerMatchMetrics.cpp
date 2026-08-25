#include <dxa/game_server/ServerMatchMetrics.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace dxa::game_server
{
namespace
{
constexpr std::size_t MaximumInitialReserve = 4096U;

[[nodiscard]] std::chrono::nanoseconds NearestRankP95(
    std::vector<std::chrono::nanoseconds> samples)
{
    if (samples.empty())
    {
        return {};
    }
    std::sort(samples.begin(), samples.end());
    const std::size_t rank = samples.size() - samples.size() / 20U;
    return samples[rank - 1U];
}

[[nodiscard]] std::uint64_t AddWithoutOverflow(
    const std::uint64_t left,
    const std::uint64_t right)
{
    if (right > std::numeric_limits<std::uint64_t>::max() - left)
    {
        throw std::overflow_error{"server match metric byte total overflow"};
    }
    return left + right;
}
} // namespace

ServerMatchMetrics::ServerMatchMetrics(const std::size_t maximumSamples)
    : ServerMatchMetrics{
          dxa::protocol::MatchId{},
          maximumSamples,
          maximumSamples}
{
}

ServerMatchMetrics::ServerMatchMetrics(
    const dxa::protocol::MatchId match,
    const std::size_t maximumTickSamples,
    const std::size_t maximumReplicationSamples)
    : match_{match},
      maximumTickSamples_{maximumTickSamples},
      maximumReplicationSamples_{maximumReplicationSamples}
{
    if (maximumTickSamples_ == 0U || maximumReplicationSamples_ == 0U)
    {
        throw std::invalid_argument{
            "server match metric capacities must be positive"};
    }
    tickSamples_.reserve(std::min(maximumTickSamples_, MaximumInitialReserve));
    replicationSamples_.reserve(std::min(
        maximumReplicationSamples_,
        MaximumInitialReserve));
}

void ServerMatchMetrics::RecordTick(const std::chrono::nanoseconds duration)
{
    if (duration < std::chrono::nanoseconds::zero())
    {
        throw std::invalid_argument{"server tick duration must be nonnegative"};
    }
    if (tickSamples_.size() >= maximumTickSamples_)
    {
        throw std::overflow_error{"server tick metric capacity exceeded"};
    }
    tickSamples_.push_back(duration);
}

void ServerMatchMetrics::RecordReplication(
    const std::chrono::nanoseconds encodeDuration,
    const std::uint32_t payloadBytes,
    const std::uint16_t fragmentCount,
    const bool keyframe,
    const std::uint16_t visibleActors,
    const std::uint16_t visibleLoot,
    const bool fallbackKeyframe)
{
    if (encodeDuration < std::chrono::nanoseconds::zero())
    {
        throw std::invalid_argument{
            "server replication duration must be nonnegative"};
    }
    if (replicationSamples_.size() >= maximumReplicationSamples_)
    {
        throw std::overflow_error{
            "server replication metric capacity exceeded"};
    }
    payloadBytes_ = AddWithoutOverflow(payloadBytes_, payloadBytes);
    replicationSamples_.push_back(ReplicationMetricSample{
        encodeDuration,
        payloadBytes,
        fragmentCount,
        visibleActors,
        visibleLoot,
        keyframe,
        fallbackKeyframe});
}

ServerMatchMetricsSnapshot ServerMatchMetrics::Snapshot(
    const dxa::game_common::GameTrafficTotals traffic,
    const std::uint64_t schedulerOverruns) const
{
    ServerMatchMetricsSnapshot snapshot;
    snapshot.match = match_;
    snapshot.tickSamples = tickSamples_;
    snapshot.replicationSamples = replicationSamples_;
    snapshot.tickP95 = NearestRankP95(tickSamples_);
    std::vector<std::chrono::nanoseconds> replicationDurations;
    replicationDurations.reserve(replicationSamples_.size());
    for (const ReplicationMetricSample& sample : replicationSamples_)
    {
        replicationDurations.push_back(sample.encodeDuration);
    }
    snapshot.replicationP95 = NearestRankP95(
        std::move(replicationDurations));
    snapshot.tcpBytes = AddWithoutOverflow(
        traffic.tcpSentBytes,
        traffic.tcpReceivedBytes);
    snapshot.udpBytes = AddWithoutOverflow(
        traffic.udpSentBytes,
        traffic.udpReceivedBytes);
    snapshot.payloadBytes = payloadBytes_;
    snapshot.schedulerOverruns = schedulerOverruns;
    return snapshot;
}
} // namespace dxa::game_server
