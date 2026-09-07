#include "pch.h"
#include "FileUtils.h"
#include "Define.h"
#include "GameRunOptions.h"
#include <filesystem>
#include <iostream>

void require(bool ok, const char* message) {
    if (!ok) { std::cerr << message << std::endl; std::exit(1); }
}

int main() {
    int calls = 0;
    CHECK([&] { ++calls; return S_OK; }());
    require(calls == 1, "Release removed a graphics operation inside CHECK");
    bool graphicsFailureRejected = false;
    try { CHECK(E_FAIL); } catch (const std::runtime_error&) { graphicsFailureRejected = true; }
    require(graphicsFailureRejected, "Release graphics failure was ignored");
    const auto path = std::filesystem::absolute("roundtrip.bin");
    const std::string text("asset\0name", 10);
    {
        FileUtils file;
        file.Open(path.wstring(), FileMode::Write);
        file.Write<uint32>(0x12345678);
        file.Write(text);
        file.Write(std::string{});
    }
    require(std::filesystem::file_size(path) == 22, "Release removed the actual file writes");
    {
        FileUtils file;
        file.Open(path.wstring(), FileMode::Read);
        require(file.Read<uint32>() == 0x12345678, "Release file read did not return stored bytes");
        std::string actual;
        file.Read(actual);
        require(actual == text, "Binary string did not retain its complete contents");
        file.Read(actual);
        require(actual.empty(), "An empty string retained the previous value");
        bool rejected = false;
        try { file.Read<uint32>(); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "Truncated file read was reported as success");
    }
    bool missingRejected = false;
    try { FileUtils file; file.Open(L"missing.bin", FileMode::Read); }
    catch (const std::runtime_error&) { missingRejected = true; }
    require(missingRejected, "Missing file was reported as open");
    const auto output = std::filesystem::absolute("frames with spaces.csv").wstring();
    const auto options = GameRunOptions::Parse({L"--resolution=1920x1080", L"--measure-output=" + output, L"--measure-seconds=1800"});
    require(options.width == 1920 && options.measurementSeconds == 1800 && options.measurementFile == output,
        "Measurement options were parsed incorrectly");
    bool invalidRejected = false;
    try { GameRunOptions::Parse({L"--measure-seconds=0", L"--measure-output=" + output}); }
    catch (const std::exception&) { invalidRejected = true; }
    require(invalidRejected, "Invalid measurement duration was accepted");
}
