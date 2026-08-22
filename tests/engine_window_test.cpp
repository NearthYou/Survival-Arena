#include <dxa/engine/InputState.hpp>
#include <dxa/engine/Window.hpp>

#include <gtest/gtest.h>

#include <Windows.h>

namespace
{
void DrainThreadMessages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE)
    {
    }
}

TEST(Window, AltF4RequestsCloseThroughDefaultSystemKeyHandling)
{
    DrainThreadMessages();

    dxa::engine::InputState input;
    dxa::engine::Window window;
    window.Create(dxa::engine::WindowConfig{L"AltF4 test", 320, 180, true}, input);

    constexpr LPARAM AltKeyContext = static_cast<LPARAM>(1) << 29;
    ASSERT_NE(FALSE, PostMessageW(window.NativeHandle(), WM_SYSKEYDOWN, VK_F4, AltKeyContext));

    EXPECT_FALSE(window.PumpMessages());
}

TEST(Window, FocusLossReleasesHeldKeysThroughMessageBoundary)
{
    DrainThreadMessages();

    dxa::engine::InputState input;
    dxa::engine::Window window;
    window.Create(dxa::engine::WindowConfig{L"Focus loss test", 320, 180, true}, input);

    input.BeginFrame();
    ASSERT_NE(FALSE, PostMessageW(window.NativeHandle(), WM_KEYDOWN, 'W', 0));
    ASSERT_TRUE(window.PumpMessages());
    ASSERT_TRUE(input.IsDown('W'));

    input.BeginFrame();
    ASSERT_NE(FALSE, PostMessageW(window.NativeHandle(), WM_KILLFOCUS, 0, 0));
    ASSERT_TRUE(window.PumpMessages());

    EXPECT_FALSE(input.IsDown('W'));
    EXPECT_TRUE(input.WasReleased('W'));
}

TEST(Window, OwnerTeardownDoesNotQuitTheNextWindow)
{
    DrainThreadMessages();

    dxa::engine::InputState firstInput;
    {
        dxa::engine::Window firstWindow;
        firstWindow.Create(
            dxa::engine::WindowConfig{L"First owner teardown test", 320, 180, true},
            firstInput);
    }

    dxa::engine::InputState secondInput;
    {
        dxa::engine::Window secondWindow;
        secondWindow.Create(
            dxa::engine::WindowConfig{L"Second owner teardown test", 320, 180, true},
            secondInput);
        EXPECT_TRUE(secondWindow.PumpMessages());
    }

    DrainThreadMessages();
}
} // namespace
