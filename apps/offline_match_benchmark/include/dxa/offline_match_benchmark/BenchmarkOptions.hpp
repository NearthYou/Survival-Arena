#pragma once

#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace dxa::offline_match_benchmark
{
struct OfflineMatchBenchmarkOptions
{
    std::string outputDirectory;
    std::string commitSha;
    std::uint32_t seed = 20260823U;
};

struct OfflineMatchBenchmarkOptionsParseResult
{
    std::optional<OfflineMatchBenchmarkOptions> options;
    std::string error;
};

namespace detail
{
[[nodiscard]] inline OfflineMatchBenchmarkOptionsParseResult Error(
    std::string message)
{
    return {std::nullopt, std::move(message)};
}

[[nodiscard]] inline std::optional<std::uint32_t> ParseUnsigned(
    const std::string_view value) noexcept
{
    std::uint32_t parsed = 0;
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end)
    {
        return std::nullopt;
    }
    return parsed;
}
} // namespace detail

[[nodiscard]] inline OfflineMatchBenchmarkOptionsParseResult
ParseOfflineMatchBenchmarkOptions(
    const std::span<const std::string_view> arguments)
{
    OfflineMatchBenchmarkOptions options;
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const std::string_view argument = arguments[index];
        if (argument == "--output" || argument == "--commit-sha")
        {
            if (index + 1U >= arguments.size() || arguments[index + 1U].empty())
            {
                return detail::Error(std::string{argument} + " requires a value");
            }
            const std::string_view value = arguments[++index];
            if (argument == "--output")
            {
                options.outputDirectory = value;
            }
            else
            {
                options.commitSha = value;
            }
        }
        else if (argument == "--seed")
        {
            if (index + 1U >= arguments.size())
            {
                return detail::Error("--seed requires a value");
            }
            const auto parsed = detail::ParseUnsigned(arguments[++index]);
            if (!parsed.has_value())
            {
                return detail::Error("--seed must be an unsigned integer");
            }
            options.seed = *parsed;
        }
        else
        {
            return detail::Error("unknown argument: " + std::string{argument});
        }
    }

    if (options.outputDirectory.empty())
    {
        return detail::Error("offline match benchmark requires --output");
    }
    if (options.commitSha.empty())
    {
        return detail::Error("offline match benchmark requires --commit-sha");
    }
    return {options, {}};
}

[[nodiscard]] inline OfflineMatchBenchmarkOptionsParseResult
ParseOfflineMatchBenchmarkOptions(
    const std::initializer_list<std::string_view> arguments)
{
    return ParseOfflineMatchBenchmarkOptions(
        std::span<const std::string_view>{arguments.begin(), arguments.size()});
}
} // namespace dxa::offline_match_benchmark
