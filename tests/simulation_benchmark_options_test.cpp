#include <dxa/simulation_benchmark/BenchmarkOptions.hpp>

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

namespace
{
using dxa::simulation_benchmark::ParseSimulationBenchmarkOptions;

TEST(SimulationBenchmarkOptions, ParsesLockedDefaultWorkload)
{
    const std::vector<std::string_view> arguments{
        "--output",
        "run",
        "--commit-sha",
        "abc",
        "--seed",
        "20260823"};

    const auto result = ParseSimulationBenchmarkOptions(arguments);

    ASSERT_TRUE(result.options.has_value());
    EXPECT_EQ("run", result.options->outputDirectory);
    EXPECT_EQ("abc", result.options->commitSha);
    EXPECT_EQ(20260823U, result.options->seed);
    EXPECT_EQ(100000U, result.options->navQueryCount);
    EXPECT_EQ(20000U, result.options->aabbQueryCount);
    EXPECT_EQ(20000U, result.options->pickQueryCount);
    EXPECT_EQ(100000U, result.options->aiDecisionCount);
    EXPECT_EQ(5U, result.options->sampleCount);
}

TEST(SimulationBenchmarkOptions, RejectsMissingRequiredAndMalformedValues)
{
    EXPECT_FALSE(ParseSimulationBenchmarkOptions({}).options.has_value());
    EXPECT_EQ(
        "simulation benchmark requires --output",
        ParseSimulationBenchmarkOptions({"--commit-sha", "abc"}).error);
    EXPECT_EQ(
        "simulation benchmark requires --commit-sha",
        ParseSimulationBenchmarkOptions({"--output", "run"}).error);
    EXPECT_EQ(
        "--seed must be an unsigned integer",
        ParseSimulationBenchmarkOptions({
            "--output",
            "run",
            "--commit-sha",
            "abc",
            "--seed",
            "nope"}).error);
    EXPECT_EQ(
        "unknown argument: --extra",
        ParseSimulationBenchmarkOptions({
            "--output",
            "run",
            "--commit-sha",
            "abc",
            "--extra"}).error);
}
} // namespace
