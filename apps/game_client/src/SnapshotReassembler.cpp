#include <dxa/game_client/SnapshotReassembler.hpp>

#include <dxa/protocol/Crc32.hpp>
#include <dxa/protocol/GameTypes.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace dxa::game_client
{
namespace
{
[[nodiscard]] std::size_t ExpectedFragmentCount(
    const std::size_t fullPayloadBytes) noexcept
{
    return fullPayloadBytes == 0U
        ? 0U
        : (fullPayloadBytes
           + dxa::protocol::MaxSnapshotFragmentPayloadBytes - 1U)
            / dxa::protocol::MaxSnapshotFragmentPayloadBytes;
}

[[nodiscard]] bool ValidFragment(
    const dxa::protocol::SnapshotFragment& fragment) noexcept
{
    if (fragment.snapshotId == 0U
        || fragment.fullPayloadBytes == 0U
        || fragment.fullPayloadBytes
            > dxa::protocol::MaxSnapshotPayloadBytes
        || fragment.fragmentCount == 0U
        || fragment.fragmentCount > dxa::protocol::MaxSnapshotFragments
        || fragment.fragmentIndex >= fragment.fragmentCount
        || fragment.fragmentCount
            != ExpectedFragmentCount(fragment.fullPayloadBytes))
    {
        return false;
    }
    const std::size_t offset =
        static_cast<std::size_t>(fragment.fragmentIndex)
        * dxa::protocol::MaxSnapshotFragmentPayloadBytes;
    if (offset >= fragment.fullPayloadBytes)
    {
        return false;
    }
    const std::size_t expectedBytes = std::min(
        dxa::protocol::MaxSnapshotFragmentPayloadBytes,
        static_cast<std::size_t>(fragment.fullPayloadBytes) - offset);
    return fragment.bytes.size() == expectedBytes;
}
} // namespace

struct SnapshotReassembler::Impl
{
    struct Assembly
    {
        dxa::protocol::MatchId match;
        std::uint32_t snapshotId = 0U;
        std::uint32_t serverTick = 0U;
        std::uint32_t ackInputSequence = 0U;
        std::uint16_t fragmentCount = 0U;
        std::uint32_t fullPayloadBytes = 0U;
        std::uint32_t fullPayloadCrc32 = 0U;
        std::vector<std::optional<std::vector<std::byte>>> fragments;
    };

    [[nodiscard]] bool MetadataMatches(
        const dxa::protocol::SnapshotFragment& fragment) const noexcept
    {
        return active.has_value()
            && active->match == fragment.match
            && active->snapshotId == fragment.snapshotId
            && active->serverTick == fragment.serverTick
            && active->ackInputSequence == fragment.ackInputSequence
            && active->fragmentCount == fragment.fragmentCount
            && active->fullPayloadBytes == fragment.fullPayloadBytes
            && active->fullPayloadCrc32 == fragment.fullPayloadCrc32;
    }

    void Begin(const dxa::protocol::SnapshotFragment& fragment)
    {
        active = Assembly{
            fragment.match,
            fragment.snapshotId,
            fragment.serverTick,
            fragment.ackInputSequence,
            fragment.fragmentCount,
            fragment.fullPayloadBytes,
            fragment.fullPayloadCrc32,
            std::vector<std::optional<std::vector<std::byte>>>(
                fragment.fragmentCount)};
        highestSnapshotId = fragment.snapshotId;
    }

    void NoteReplacement(const std::uint32_t snapshotId) noexcept
    {
        if (!highestSnapshotId.has_value())
        {
            return;
        }
        const bool skippedSnapshot = snapshotId > *highestSnapshotId
            && snapshotId - *highestSnapshotId > 1U;
        if (active.has_value() || skippedSnapshot)
        {
            recoveryNeeded = true;
        }
    }

    std::optional<std::uint32_t> highestSnapshotId;
    std::optional<Assembly> active;
    bool recoveryNeeded = false;
};

SnapshotReassembler::SnapshotReassembler()
    : impl_{std::make_unique<Impl>()}
{
}

SnapshotReassembler::~SnapshotReassembler() = default;
SnapshotReassembler::SnapshotReassembler(SnapshotReassembler&&) noexcept =
    default;
SnapshotReassembler& SnapshotReassembler::operator=(
    SnapshotReassembler&&) noexcept = default;

std::optional<ReassembledPayload> SnapshotReassembler::PushBytes(
    const dxa::protocol::SnapshotFragment& fragment)
{
    if (impl_ == nullptr)
    {
        return std::nullopt;
    }
    Impl& state = *impl_;
    if (!ValidFragment(fragment))
    {
        if (!state.highestSnapshotId.has_value()
            || fragment.snapshotId > *state.highestSnapshotId)
        {
            state.NoteReplacement(fragment.snapshotId);
            state.highestSnapshotId = fragment.snapshotId;
            state.active.reset();
        }
        return std::nullopt;
    }

    if (state.highestSnapshotId.has_value()
        && fragment.snapshotId < *state.highestSnapshotId)
    {
        return std::nullopt;
    }
    if (!state.highestSnapshotId.has_value()
        || fragment.snapshotId > *state.highestSnapshotId)
    {
        state.NoteReplacement(fragment.snapshotId);
        state.Begin(fragment);
    }
    else if (!state.active.has_value())
    {
        return std::nullopt;
    }
    else if (!state.MetadataMatches(fragment))
    {
        state.recoveryNeeded = true;
        state.active.reset();
        return std::nullopt;
    }

    auto& destination = state.active->fragments[fragment.fragmentIndex];
    if (destination.has_value())
    {
        if (*destination != fragment.bytes)
        {
            state.recoveryNeeded = true;
            state.active.reset();
        }
        return std::nullopt;
    }
    destination = fragment.bytes;
    const bool complete = std::all_of(
        state.active->fragments.begin(),
        state.active->fragments.end(),
        [](const auto& bytes) { return bytes.has_value(); });
    if (!complete)
    {
        return std::nullopt;
    }

    std::vector<std::byte> payload;
    payload.reserve(state.active->fullPayloadBytes);
    for (const auto& bytes : state.active->fragments)
    {
        payload.insert(payload.end(), bytes->begin(), bytes->end());
    }
    const Impl::Assembly metadata = std::move(*state.active);
    state.active.reset();
    if (payload.size() != metadata.fullPayloadBytes
        || dxa::protocol::Crc32(payload) != metadata.fullPayloadCrc32)
    {
        state.recoveryNeeded = true;
        return std::nullopt;
    }
    return ReassembledPayload{
        metadata.snapshotId,
        metadata.serverTick,
        metadata.ackInputSequence,
        std::move(payload)};
}

bool SnapshotReassembler::TakeRecoveryNeeded() noexcept
{
    if (!impl_)
    {
        return false;
    }
    const bool needed = impl_->recoveryNeeded;
    impl_->recoveryNeeded = false;
    return needed;
}

void SnapshotReassembler::Reset() noexcept
{
    if (impl_)
    {
        impl_->highestSnapshotId.reset();
        impl_->active.reset();
        impl_->recoveryNeeded = false;
    }
}
} // namespace dxa::game_client
