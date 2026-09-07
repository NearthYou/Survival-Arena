#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// Records only the game's back buffer. No desktop pixels or audio are read.
class ReferenceVideoCapture
{
    using Clock = std::chrono::steady_clock;
public:
    ~ReferenceVideoCapture() { Finish(); }

    template<class ReadFrame>
    void Record(ReadFrame read)
    {
        const auto now = Clock::now();
        if (!m_process)
        {
            if (FAILED(read(m_pixels, m_width, m_height))) Fail("Video readback failed");
            Start();
            m_start = now;
            WriteFrame();
            return;
        }
        const auto wanted = static_cast<uint64_t>(std::chrono::duration<double>(now - m_start).count() * 30) + 1;
        if (wanted <= m_frames) return;
        // A loading pause repeats the last displayed frame instead of speeding up time.
        while (m_frames + 1 < wanted) WriteFrame();
        UINT width = 0, height = 0;
        const auto readStart = Clock::now();
        const auto readResult = read(m_pixels, width, height);
        m_readMs += std::chrono::duration<double,std::milli>(Clock::now()-readStart).count();
        ++m_reads;
        if (readResult == S_FALSE) ++m_pendingReads;
        if (FAILED(readResult) || width != m_width || height != m_height)
            Fail("Video back buffer changed");
        WriteFrame();
    }

    void Mark(const wchar_t* name)
    {
        if (!name) return;
        const auto seconds = m_process ? std::chrono::duration<double>(Clock::now() - m_start).count() : 0.;
        std::ofstream events("video-events.csv", std::ios::app);
        events << seconds << ',';
        for (auto p = name; *p; ++p) events << static_cast<char>(*p);
        events << '\n';
    }

private:
    [[noreturn]] void Fail(const char* message)
    {
        m_failed = true;
        throw std::runtime_error(message);
    }

    void Start()
    {
        if (std::filesystem::exists("actual-play.mp4")) Fail("Video output already exists");
        SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE input = nullptr;
        if (!CreatePipe(&input, &m_pipe, &attributes, 1024 * 1024)) Fail("Cannot create encoder pipe");
        if (!SetHandleInformation(m_pipe, HANDLE_FLAG_INHERIT, 0))
        { CloseHandle(input); Fail("Cannot isolate the encoder pipe"); }
        const auto log = CreateFileW(L"video-encoder.log", GENERIC_WRITE, FILE_SHARE_READ,
            &attributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (log == INVALID_HANDLE_VALUE) { CloseHandle(input); Fail("Cannot open encoder log"); }
        const std::wstring encoder = DXA_GAME_VIDEO_ENCODER;
        const std::wstring codec = DXA_GAME_VIDEO_CODEC;
        const std::wstring options = codec == L"h264_nvenc"
            ? L" -c:v h264_nvenc -preset p4 -cq 23"
            : L" -c:v libx264 -preset ultrafast -crf 20 -threads 2";
        std::wstring command = L"\"" + encoder + L"\" -hide_banner -loglevel warning -n -f rawvideo -pixel_format bgra -video_size "
            + std::to_wstring(m_width) + L"x" + std::to_wstring(m_height)
            + L" -framerate 30 -i pipe:0 -an" + options + L" -pix_fmt yuv420p -movflags +faststart actual-play.mp4";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        startup.hStdInput = input;
        startup.hStdOutput = startup.hStdError = log;
        PROCESS_INFORMATION process{};
        const auto started = CreateProcessW(encoder.c_str(), command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
        CloseHandle(input);
        CloseHandle(log);
        if (!started) Fail("Cannot launch video encoder");
        CloseHandle(process.hThread);
        m_process = process.hProcess;
    }

    void WriteFrame()
    {
        const auto begin = Clock::now();
        size_t offset = 0;
        while (offset < m_pixels.size())
        {
            DWORD written = 0;
            if (!WriteFile(m_pipe, m_pixels.data() + offset, static_cast<DWORD>(m_pixels.size() - offset), &written, nullptr) || !written)
                Fail("Video encoder stopped receiving frames");
            offset += written;
        }
        ++m_frames;
        m_writeMs += std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
    }

    void Finish()
    {
        if (m_pipe) { CloseHandle(m_pipe); m_pipe = nullptr; }
        if (!m_process) return;
        DWORD exitCode = 1;
        if (WaitForSingleObject(m_process, 60000) == WAIT_OBJECT_0)
            GetExitCodeProcess(m_process, &exitCode);
        else { TerminateProcess(m_process, 1); m_failed = true; }
        CloseHandle(m_process);
        m_process = nullptr;
        std::ofstream result("video-capture.json");
        result << "{\"passed\":" << (!m_failed && exitCode == 0 && m_frames > 0 ? "true" : "false")
            << ",\"encoder_exit\":" << exitCode << ",\"frames\":" << m_frames
            << ",\"fps\":30,\"width\":" << m_width << ",\"height\":" << m_height << ",\"audio\":false"
            << ",\"readbacks\":" << m_reads << ",\"pending_readbacks\":" << m_pendingReads
            << ",\"mean_readback_ms\":" << (m_reads ? m_readMs/m_reads : 0)
            << ",\"mean_pipe_write_ms\":" << (m_frames ? m_writeMs/m_frames : 0) << "}";
    }

    HANDLE m_pipe = nullptr, m_process = nullptr;
    Clock::time_point m_start{};
    std::vector<std::uint8_t> m_pixels;
    uint64_t m_frames = 0;
    uint64_t m_reads = 0, m_pendingReads = 0;
    double m_readMs = 0, m_writeMs = 0;
    UINT m_width = 0, m_height = 0;
    bool m_failed = false;
};

inline ReferenceVideoCapture& ReferenceVideo()
{
    static ReferenceVideoCapture capture;
    return capture;
}
