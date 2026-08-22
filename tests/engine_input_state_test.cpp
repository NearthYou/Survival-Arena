#include <dxa/engine/InputState.hpp>

#include <gtest/gtest.h>

namespace
{
using dxa::engine::InputState;

constexpr std::uint8_t MoveForward = 0x57;

TEST(InputState, ReportsPressedOnlyOnTransition)
{
    InputState input;
    input.SetKey(MoveForward, true);

    EXPECT_TRUE(input.IsDown(MoveForward));
    EXPECT_TRUE(input.WasPressed(MoveForward));
    EXPECT_FALSE(input.WasReleased(MoveForward));

    input.BeginFrame();

    EXPECT_TRUE(input.IsDown(MoveForward));
    EXPECT_FALSE(input.WasPressed(MoveForward));
}

TEST(InputState, ReportsReleasedOnlyOnTransition)
{
    InputState input;
    input.SetKey(MoveForward, true);
    input.BeginFrame();

    input.SetKey(MoveForward, false);

    EXPECT_FALSE(input.IsDown(MoveForward));
    EXPECT_TRUE(input.WasReleased(MoveForward));

    input.BeginFrame();
    EXPECT_FALSE(input.WasReleased(MoveForward));
}

TEST(InputState, PreservesPressAndReleaseWithinOneFrame)
{
    InputState input;

    input.SetKey(MoveForward, true);
    input.SetKey(MoveForward, false);

    EXPECT_FALSE(input.IsDown(MoveForward));
    EXPECT_TRUE(input.WasPressed(MoveForward));
    EXPECT_TRUE(input.WasReleased(MoveForward));
}

TEST(InputState, ReleasesHeldKeysWhenFocusIsLost)
{
    InputState input;
    input.SetKey(MoveForward, true);
    input.BeginFrame();

    input.ReleaseAll();

    EXPECT_FALSE(input.IsDown(MoveForward));
    EXPECT_TRUE(input.WasReleased(MoveForward));
}
} // namespace
