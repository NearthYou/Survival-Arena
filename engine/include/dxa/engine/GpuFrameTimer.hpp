#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace dxa::engine
{
struct GpuFrameResult
{
    std::uint64_t frameIndex = 0;
    std::optional<double> elapsedMilliseconds;
};

class GpuFrameTimer
{
public:
    void Initialize(ID3D11Device* device);
    [[nodiscard]] bool BeginFrame(
        ID3D11DeviceContext* context,
        std::uint64_t frameIndex);
    void EndFrame(ID3D11DeviceContext* context);
    [[nodiscard]] std::vector<GpuFrameResult> ResolveReady(
        ID3D11DeviceContext* context);
    [[nodiscard]] std::vector<GpuFrameResult> Drain(
        ID3D11DeviceContext* context,
        std::chrono::milliseconds timeout);

private:
    static constexpr std::size_t QuerySlotCount = 16;

    struct QuerySlot
    {
        Microsoft::WRL::ComPtr<ID3D11Query> disjoint;
        Microsoft::WRL::ComPtr<ID3D11Query> start;
        Microsoft::WRL::ComPtr<ID3D11Query> end;
        std::uint64_t frameIndex = 0;
        bool pending = false;
    };

    [[nodiscard]] static bool TryResolve(
        ID3D11DeviceContext* context,
        QuerySlot& slot,
        GpuFrameResult& result);
    [[nodiscard]] bool HasPendingQueries() const noexcept;

    std::array<QuerySlot, QuerySlotCount> slots_;
    std::optional<std::size_t> activeSlot_;
    std::size_t nextSlot_ = 0;
    bool initialized_ = false;
};
} // namespace dxa::engine
