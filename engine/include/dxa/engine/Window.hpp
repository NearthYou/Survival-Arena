#pragma once

#include <dxa/engine/InputState.hpp>

#include <Windows.h>

#include <cstdint>
#include <string>

namespace dxa::engine
{
struct WindowConfig
{
    std::wstring title = L"DX11 Survival Arena";
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    bool hidden = false;
};

class Window
{
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void Create(const WindowConfig& config, InputState& inputState);
    void SetTitle(const std::wstring& title);
    [[nodiscard]] bool PumpMessages() const noexcept;
    [[nodiscard]] HWND NativeHandle() const noexcept;

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    InputState* inputState_ = nullptr;
    bool ownerTeardown_ = false;
};
} // namespace dxa::engine
