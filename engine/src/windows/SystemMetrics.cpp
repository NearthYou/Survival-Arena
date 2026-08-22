#include <dxa/engine/SystemMetrics.hpp>

#include <Windows.h>
#include <dxgi.h>
#include <psapi.h>
#include <wrl/client.h>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace dxa::engine
{
std::uint64_t GetCurrentProcessWorkingSetBytes()
{
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters)) == FALSE)
    {
        throw std::runtime_error{"GetProcessMemoryInfo failed"};
    }
    return static_cast<std::uint64_t>(counters.WorkingSetSize);
}

std::string GetAdapterNameUtf8(ID3D11Device* const device)
{
    if (device == nullptr)
    {
        throw std::invalid_argument{"adapter description requires a Direct3D device"};
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()))))
    {
        throw std::runtime_error{"ID3D11Device::QueryInterface(IDXGIDevice) failed"};
    }
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDevice->GetAdapter(adapter.GetAddressOf())))
    {
        throw std::runtime_error{"IDXGIDevice::GetAdapter failed"};
    }
    DXGI_ADAPTER_DESC description{};
    if (FAILED(adapter->GetDesc(&description)))
    {
        throw std::runtime_error{"IDXGIAdapter::GetDesc failed"};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        description.Description,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (size <= 1)
    {
        throw std::runtime_error{"adapter description UTF-8 conversion failed"};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            description.Description,
            -1,
            result.data(),
            size,
            nullptr,
            nullptr) == 0)
    {
        throw std::runtime_error{"adapter description UTF-8 conversion failed"};
    }
    result.pop_back();
    return result;
}
} // namespace dxa::engine
