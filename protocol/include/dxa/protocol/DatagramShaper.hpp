#pragma once

#include <chrono>
#include <cstdint>

namespace dxa::protocol
{
struct DatagramShaperConfig
{
    std::chrono::milliseconds oneWayLatency{0};
    std::chrono::milliseconds jitter{0};
    std::uint32_t lossBasisPoints = 0U;
    std::uint32_t seed = 0U;
};

enum class DatagramDirection : std::uint8_t
{
    ClientToServer = 1,
    ServerToClient = 2
};

struct ShapedDatagramDecision
{
    bool drop = false;
    std::chrono::milliseconds delay{0};

    [[nodiscard]] bool operator==(
        const ShapedDatagramDecision&) const = default;
};

class DatagramShaper
{
public:
    DatagramShaper(
        DatagramShaperConfig config,
        DatagramDirection direction);

    [[nodiscard]] ShapedDatagramDecision Decide(
        std::uint64_t peerKey,
        std::uint64_t ordinal) const noexcept;
    [[nodiscard]] bool Enabled() const noexcept;

private:
    DatagramShaperConfig config_;
    DatagramDirection direction_ = DatagramDirection::ClientToServer;
    bool enabled_ = false;
};
} // namespace dxa::protocol
