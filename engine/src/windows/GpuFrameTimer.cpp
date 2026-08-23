#include <dxa/engine/GpuFrameTimer.hpp>

#include <chrono>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace dxa::engine
{
namespace
{
void RequireSuccess(const HRESULT result, const char* operation)
{
    if (FAILED(result))
    {
        std::ostringstream message;
        message << operation << " failed with HRESULT 0x" << std::hex << std::uppercase
                << static_cast<std::uint32_t>(result);
        throw std::runtime_error{message.str()};
    }
}
} // namespace

void GpuFrameTimer::Initialize(
    ID3D11Device* const device,
    const std::size_t querySlotCount)
{
    if (device == nullptr
        || querySlotCount == 0
        || querySlotCount > MaximumQuerySlotCount)
    {
        throw std::invalid_argument{
            "GPU frame timer requires a device and a supported non-zero slot count"};
    }

    const D3D11_QUERY_DESC disjointDescription{
        D3D11_QUERY_TIMESTAMP_DISJOINT,
        0};
    const D3D11_QUERY_DESC timestampDescription{
        D3D11_QUERY_TIMESTAMP,
        0};
    slots_.clear();
    slots_.resize(querySlotCount);
    for (QuerySlot& slot : slots_)
    {
        slot = {};
        RequireSuccess(
            device->CreateQuery(
                &disjointDescription,
                slot.disjoint.ReleaseAndGetAddressOf()),
            "ID3D11Device::CreateQuery(timestamp disjoint)");
        RequireSuccess(
            device->CreateQuery(
                &timestampDescription,
                slot.start.ReleaseAndGetAddressOf()),
            "ID3D11Device::CreateQuery(timestamp start)");
        RequireSuccess(
            device->CreateQuery(
                &timestampDescription,
                slot.end.ReleaseAndGetAddressOf()),
            "ID3D11Device::CreateQuery(timestamp end)");
    }
    activeSlot_.reset();
    nextSlot_ = 0;
    initialized_ = true;
}

bool GpuFrameTimer::BeginFrame(
    ID3D11DeviceContext* const context,
    const std::uint64_t frameIndex)
{
    if (!initialized_ || context == nullptr || frameIndex == 0)
    {
        throw std::invalid_argument{
            "GPU frame timing requires initialization, a context, and a one-based frame index"};
    }
    if (activeSlot_.has_value())
    {
        throw std::logic_error{"GPU frame timing is already active"};
    }

    for (std::size_t offset = 0; offset < slots_.size(); ++offset)
    {
        const std::size_t index = (nextSlot_ + offset) % slots_.size();
        QuerySlot& slot = slots_[index];
        if (slot.pending)
        {
            continue;
        }

        slot.frameIndex = frameIndex;
        context->Begin(slot.disjoint.Get());
        context->End(slot.start.Get());
        activeSlot_ = index;
        nextSlot_ = (index + 1) % slots_.size();
        return true;
    }
    return false;
}

void GpuFrameTimer::EndFrame(ID3D11DeviceContext* const context)
{
    if (context == nullptr || !activeSlot_.has_value())
    {
        throw std::logic_error{"GPU frame timing has no active query"};
    }

    QuerySlot& slot = slots_[*activeSlot_];
    context->End(slot.end.Get());
    context->End(slot.disjoint.Get());
    slot.pending = true;
    activeSlot_.reset();
}

bool GpuFrameTimer::TryResolve(
    ID3D11DeviceContext* const context,
    QuerySlot& slot,
    GpuFrameResult& result)
{
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
    const HRESULT disjointResult = context->GetData(
        slot.disjoint.Get(),
        &disjoint,
        sizeof(disjoint),
        D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (disjointResult == S_FALSE)
    {
        return false;
    }
    RequireSuccess(disjointResult, "ID3D11DeviceContext::GetData(timestamp disjoint)");

    std::uint64_t start = 0;
    const HRESULT startResult = context->GetData(
        slot.start.Get(),
        &start,
        sizeof(start),
        D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (startResult == S_FALSE)
    {
        return false;
    }
    RequireSuccess(startResult, "ID3D11DeviceContext::GetData(timestamp start)");

    std::uint64_t end = 0;
    const HRESULT endResult = context->GetData(
        slot.end.Get(),
        &end,
        sizeof(end),
        D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (endResult == S_FALSE)
    {
        return false;
    }
    RequireSuccess(endResult, "ID3D11DeviceContext::GetData(timestamp end)");

    result.frameIndex = slot.frameIndex;
    if (disjoint.Disjoint == FALSE && disjoint.Frequency != 0 && end >= start)
    {
        result.elapsedMilliseconds = static_cast<double>(end - start)
            * 1000.0
            / static_cast<double>(disjoint.Frequency);
    }
    else
    {
        result.elapsedMilliseconds.reset();
    }
    slot.pending = false;
    return true;
}

std::vector<GpuFrameResult> GpuFrameTimer::ResolveReady(
    ID3D11DeviceContext* const context)
{
    if (!initialized_ || context == nullptr)
    {
        throw std::invalid_argument{"GPU frame timer requires initialization and a context"};
    }

    std::vector<GpuFrameResult> results;
    for (QuerySlot& slot : slots_)
    {
        if (!slot.pending)
        {
            continue;
        }
        GpuFrameResult result;
        if (TryResolve(context, slot, result))
        {
            results.push_back(result);
        }
    }
    return results;
}

std::vector<GpuFrameResult> GpuFrameTimer::Drain(
    ID3D11DeviceContext* const context,
    const std::chrono::milliseconds timeout)
{
    if (!initialized_ || context == nullptr || timeout < std::chrono::milliseconds::zero())
    {
        throw std::invalid_argument{"GPU frame timer drain requires a context and non-negative timeout"};
    }
    if (activeSlot_.has_value())
    {
        throw std::logic_error{"GPU frame timer cannot drain an active query"};
    }

    context->Flush();
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::vector<GpuFrameResult> results;
    while (HasPendingQueries())
    {
        auto ready = ResolveReady(context);
        results.insert(results.end(), ready.begin(), ready.end());
        if (!HasPendingQueries() || std::chrono::steady_clock::now() >= deadline)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return results;
}

bool GpuFrameTimer::HasPendingQueries() const noexcept
{
    for (const QuerySlot& slot : slots_)
    {
        if (slot.pending)
        {
            return true;
        }
    }
    return false;
}
} // namespace dxa::engine
