#pragma once

#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "GameContentHash.h"

#pragma comment(lib, "bcrypt.lib")

namespace dxa_game_runtime
{
inline void RejectLink(const std::filesystem::path& path)
{
    for (auto current = path; !current.empty(); current = current.parent_path())
    {
        const auto attributes = GetFileAttributesW(current.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::runtime_error("Game content is missing or uses a directory link.");
        if (current == current.root_path()) break;
    }
}

inline std::string HashFile(const std::filesystem::path& path)
{
    RejectLink(path);
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read game content: " + path.u8string());
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    auto check = [](NTSTATUS status) {
        if (status < 0) throw std::runtime_error("Game content SHA256 validation failed.");
    };
    check(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0));
    try
    {
        check(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0));
        std::array<char, 65536> buffer{};
        while (input)
        {
            input.read(buffer.data(), buffer.size());
            check(BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                static_cast<ULONG>(input.gcount()), 0));
        }
        if (!input.eof()) throw std::runtime_error("Game content read failed.");
        std::array<unsigned char, 32> digest{};
        check(BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0));
        BCryptDestroyHash(hash);
        hash = nullptr;
        BCryptCloseAlgorithmProvider(algorithm, 0);
        algorithm = nullptr;
        const char digits[] = "0123456789abcdef";
        std::string result;
        for (auto value : digest) { result += digits[value >> 4]; result += digits[value & 15]; }
        return result;
    }
    catch (...)
    {
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        throw;
    }
}

inline bool Prepare(bool validateOnly)
{
    try
    {
        std::vector<wchar_t> module(32768);
        const auto length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
        if (!length || length == module.size()) throw std::runtime_error("Cannot locate dxa_game.exe.");
        const auto binaries = std::filesystem::path(std::wstring(module.data(), length)).parent_path();
        const auto root = binaries.parent_path();
        const auto manifest = root / L"content.sha256";
        if (HashFile(manifest) != DXA_GAME_CONTENT_SHA256)
            throw std::runtime_error("Game content manifest does not match this build.");
        std::ifstream input(manifest, std::ios::binary);
        std::string line;
        size_t resourceFiles = 0, shaderFiles = 0;
        while (std::getline(input, line))
        {
            if (line.size() < 66 || line[64] != '\t') throw std::runtime_error("Invalid game content manifest.");
            const auto relative = line.substr(65);
            if (HashFile(root / std::filesystem::u8path(relative)) != line.substr(0, 64))
                throw std::runtime_error("Game content hash mismatch: " + relative);
            if (relative.rfind("Resources/", 0) == 0) ++resourceFiles;
            if (relative.rfind("Shaders/", 0) == 0) ++shaderFiles;
        }
        for (const auto& folder : {std::make_pair(L"Resources", resourceFiles), std::make_pair(L"Shaders", shaderFiles)})
        {
            size_t count = 0;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root / folder.first))
            {
                RejectLink(entry.path());
                if (entry.is_regular_file()) ++count;
            }
            if (count != folder.second) throw std::runtime_error("Unexpected files in game content.");
        }
        // Keep every original relative resource/shader lookup unchanged.
        std::filesystem::current_path(binaries);
        if (validateOnly) std::cout << "Game runtime verified: " << root.u8string() << std::endl;
        return true;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Game startup failed: " << error.what() << std::endl;
        if (!validateOnly) MessageBoxA(nullptr, error.what(), "Survival Arena startup failed", MB_OK | MB_ICONERROR);
        return false;
    }
}
}
