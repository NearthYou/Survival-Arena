#include <dxa/offline_match_benchmark/BenchmarkOptions.hpp>

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

namespace
{
using dxa::offline_match_benchmark::ParseOfflineMatchBenchmarkOptions;

TEST(OfflineMatchBenchmarkOptions, ParsesRequiredEvidenceBoundary)
{
    const std::vector<std::string_view> arguments{
        "--output",
        "run",
        "--commit-sha",
        "abc",
        "--seed",
        "20260823"};

    const auto result = ParseOfflineMatchBenchmarkOptions(arguments);

    ASSERT_TRUE(result.options.has_value());
    EXPECT_EQ("run", result.options->outputDirectory);
    EXPECT_EQ("abc", result.options->commitSha);
    EXPECT_EQ(20260823U, result.options->seed);
}

TEST(OfflineMatchBenchmarkOptions, RejectsMissingAndMalformedArguments)
{
    EXPECT_EQ(
        "offline match benchmark requires --output",
        ParseOfflineMatchBenchmarkOptions({"--commit-sha", "abc"}).error);
    EXPECT_EQ(
        "offline match benchmark requires --commit-sha",
        ParseOfflineMatchBenchmarkOptions({"--output", "run"}).error);
    EXPECT_EQ(
        "--seed must be an unsigned integer",
        ParseOfflineMatchBenchmarkOptions({
            "--output", "run", "--commit-sha", "abc", "--seed", "bad"})
            .error);
    EXPECT_EQ(
        "unknown argument: --extra",
        ParseOfflineMatchBenchmarkOptions({
            "--output", "run", "--commit-sha", "abc", "--extra"})
            .error);
}
} // namespace
