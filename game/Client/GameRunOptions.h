#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

struct GameRunOptions
{
    bool validateOnly = false;
    int width = 1366;
    int height = 768;
    std::wstring measurementFile;
    unsigned measurementSeconds = 0;

    static GameRunOptions Parse(const std::vector<std::wstring>& arguments)
    {
        GameRunOptions result;
        for (const auto& arg : arguments)
        {
            if (arg == L"--validate-runtime") result.validateOnly = true;
            else if (arg == L"--resolution=1366x768") { result.width = 1366; result.height = 768; }
            else if (arg == L"--resolution=1920x1080") { result.width = 1920; result.height = 1080; }
            else if (arg.rfind(L"--measure-output=", 0) == 0)
                result.measurementFile = arg.substr(17);
            else if (arg.rfind(L"--measure-seconds=", 0) == 0)
            {
                const auto value = arg.substr(18);
                size_t end = 0;
                const auto seconds = std::stoul(value, &end);
                if (end != value.size() || seconds < 1 || seconds > 86400)
                    throw std::runtime_error("Measurement duration must be 1..86400 seconds.");
                result.measurementSeconds = static_cast<unsigned>(seconds);
            }
            else throw std::runtime_error("Unknown game argument.");
        }
        if (result.measurementFile.empty() != (result.measurementSeconds == 0))
            throw std::runtime_error("Measurement requires both output and duration.");
        if (!result.measurementFile.empty() && !std::filesystem::path(result.measurementFile).is_absolute())
            throw std::runtime_error("Measurement output must be an absolute path.");
        return result;
    }
};
