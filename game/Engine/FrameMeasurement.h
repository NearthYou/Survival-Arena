#pragma once
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

// Opt-in measurements use wall time, independently of the gameplay delta clamp.
class FrameMeasurement
{
    using Clock = std::chrono::steady_clock;
public:
    FrameMeasurement(const std::wstring& filename, unsigned seconds)
        : m_seconds(seconds), m_filename(filename)
    {
        if (std::filesystem::exists(m_filename))
            throw std::runtime_error("Measurement output already exists.");
        m_file.open(m_filename);
        if (!m_file) throw std::runtime_error("Cannot create measurement output.");
        m_file << "elapsed_seconds,frame_ms,working_set_bytes,private_bytes,handles,present_result\n" << std::setprecision(10);
    }

    void RecordAdapter(ID3D11Device* device, int width, int height)
    {
        Microsoft::WRL::ComPtr<IDXGIDevice> dxgi;
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        DXGI_ADAPTER_DESC desc{};
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(dxgi.GetAddressOf()))) ||
            FAILED(dxgi->GetAdapter(adapter.GetAddressOf())) || FAILED(adapter->GetDesc(&desc)))
            throw std::runtime_error("Cannot identify the measured GPU.");
        std::ofstream metadata(std::filesystem::path(m_filename.wstring() + L".meta.txt"));
        metadata << "adapter=";
        for (const wchar_t c : std::wstring(desc.Description)) metadata << static_cast<char>(c < 128 ? c : '?');
        metadata << "\nvendor_id=" << desc.VendorId << "\ndevice_id=" << desc.DeviceId
                 << "\nwidth=" << width << "\nheight=" << height << "\npresent_sync_interval=1\n";
    }

    bool Record(bool gameplayReady, HRESULT presentResult)
    {
        if (!gameplayReady) return false;
        const auto now = Clock::now();
        if (!m_started)
        {
            m_started = true;
            m_start = m_previous = now;
            return false;
        }
        const double elapsed = std::chrono::duration<double>(now - m_start).count();
        const double frameMs = std::chrono::duration<double, std::milli>(now - m_previous).count();
        m_previous = now;
        if (elapsed >= m_nextMemorySample)
        {
            PROCESS_MEMORY_COUNTERS_EX counters{};
            counters.cb = sizeof(counters);
            if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)) ||
                !GetProcessHandleCount(GetCurrentProcess(), &m_handles))
                throw std::runtime_error("Cannot sample game process memory.");
            m_workingSet = counters.WorkingSetSize;
            m_privateBytes = counters.PrivateUsage;
            m_nextMemorySample = elapsed + 1.;
            m_file.flush();
        }
        m_file << elapsed << ',' << frameMs << ',' << m_workingSet << ',' << m_privateBytes << ',' << m_handles << ',' << presentResult << '\n';
        if (!m_file) throw std::runtime_error("Cannot write game measurements.");
        if (elapsed < m_seconds) return false;
        m_file.flush();
        return true;
    }

private:
    unsigned m_seconds;
    std::filesystem::path m_filename;
    std::ofstream m_file;
    bool m_started = false;
    Clock::time_point m_start{}, m_previous{};
    double m_nextMemorySample = 0;
    SIZE_T m_workingSet = 0, m_privateBytes = 0;
    DWORD m_handles = 0;
};
