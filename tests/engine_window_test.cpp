#include <dxa/engine/InputState.hpp>
#include <dxa/engine/Window.hpp>

#include <gtest/gtest.h>

#include <Windows.h>

#include <array>
#include <stdexcept>
#include <string>

namespace
{
void DrainThreadMessages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE)
    {
    }
}

[[nodiscard]] std::wstring NativeWindowTitle(const HWND window)
{
    std::array<wchar_t, 256> title{};
    const int length = GetWindowTextW(
        window,
        title.data(),
        static_cast<int>(title.size()));
    if (length < 0)
    {
        throw std::runtime_error{"GetWindowTextW failed"};
    }
    return std::wstring{title.data(), static_cast<std::size_t>(length)};
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

TEST(Window, PointerMessagesCrossTheWindowBoundary)
{
    DrainThreadMessages();

    dxa::engine::InputState input;
    dxa::engine::Window window;
    window.Create(
        dxa::engine::WindowConfig{L"Pointer message test", 320, 180, true},
        input);

    input.BeginFrame();
    ASSERT_NE(
        FALSE,
        PostMessageW(window.NativeHandle(), WM_MOUSEMOVE, 0, MAKELPARAM(30, 40)));
    ASSERT_NE(
        FALSE,
        PostMessageW(
            window.NativeHandle(),
            WM_RBUTTONDOWN,
            MK_RBUTTON,
            MAKELPARAM(30, 40)));
    ASSERT_TRUE(window.PumpMessages());

    EXPECT_EQ((dxa::engine::PointerPosition{30, 40}), input.Pointer());
    EXPECT_TRUE(input.WasRightPointerPressed());
    EXPECT_TRUE(input.IsRightPointerDown());

    input.BeginFrame();
    ASSERT_NE(
        FALSE,
        PostMessageW(window.NativeHandle(), WM_RBUTTONUP, 0, MAKELPARAM(35, 45)));
    ASSERT_TRUE(window.PumpMessages());

    EXPECT_EQ((dxa::engine::PointerPosition{35, 45}), input.Pointer());
    EXPECT_FALSE(input.IsRightPointerDown());
    EXPECT_TRUE(input.WasRightPointerReleased());
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

TEST(Window, UpdatesVisibleMatchStatusTitle)
{
    DrainThreadMessages();

    dxa::engine::Window uncreated;
    EXPECT_THROW(uncreated.SetTitle(L"Before create"), std::logic_error);

    dxa::engine::InputState input;
    dxa::engine::Window window;
    window.Create(
        dxa::engine::WindowConfig{L"Initial match title", 320, 180, true},
        input);

    window.SetTitle(L"Alive 7 | Rifle");

    EXPECT_EQ(L"Alive 7 | Rifle", NativeWindowTitle(window.NativeHandle()));
    EXPECT_THROW(window.SetTitle(L""), std::invalid_argument);
}
} // namespace
