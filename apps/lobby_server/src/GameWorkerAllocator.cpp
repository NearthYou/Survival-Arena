#include <dxa/lobby/GameWorkerAllocator.hpp>

#include <algorithm>
#include <utility>

namespace dxa::lobby
{
namespace
{
[[nodiscard]] bool IsValidHost(const std::string& host) noexcept
{
    if (host.empty() || host.size() > 255U)
    {
        return false;
    }
    return std::all_of(host.begin(), host.end(), [](const char character) {
        const auto value = static_cast<unsigned char>(character);
        return value >= 0x21U && value <= 0x7EU;
    });
}
} // namespace

WorkerAllocationResult UnavailableGameWorkerAllocator::Allocate(
    const dxa::protocol::MatchId,
    const std::span<const dxa::protocol::PlayerId>)
{
    return {};
}

StaticGameWorkerAllocator::StaticGameWorkerAllocator(GameEndpoint endpoint)
    : endpoint_{std::move(endpoint)},
      valid_{IsValidHost(endpoint_.host)
          && endpoint_.tcpPort != 0U
          && endpoint_.udpPort != 0U}
{
}

WorkerAllocationResult StaticGameWorkerAllocator::Allocate(
    const dxa::protocol::MatchId,
    const std::span<const dxa::protocol::PlayerId>)
{
    if (!valid_)
    {
        return {};
    }
    return WorkerAllocationResult{endpoint_};
}
} // namespace dxa::lobby
