#include <dxa/engine/InputState.hpp>
#include <dxa/engine/Window.hpp>

#include <gtest/gtest.h>

#include <Windows.h>

namespace
{
TEST(Window, AltF4RequestsCloseThroughDefaultSystemKeyHandling)
{
    dxa::engine::InputState input;
    dxa::engine::Window window;
    window.Create(dxa::engine::WindowConfig{L"AltF4 test", 320, 180, true}, input);

    constexpr LPARAM AltKeyContext = static_cast<LPARAM>(1) << 29;
    ASSERT_NE(FALSE, PostMessageW(window.NativeHandle(), WM_SYSKEYDOWN, VK_F4, AltKeyContext));

    EXPECT_FALSE(window.PumpMessages());
}
} // namespace

