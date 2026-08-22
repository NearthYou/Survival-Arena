#include <dxa/engine/Window.hpp>

#include <stdexcept>

namespace dxa::engine
{
namespace
{
constexpr wchar_t WindowClassName[] = L"DXASurvivalArenaWindowClass";
}

Window::~Window()
{
    if (window_ != nullptr && IsWindow(window_))
    {
        DestroyWindow(window_);
    }
}

void Window::Create(const WindowConfig& config, InputState& inputState)
{
    instance_ = GetModuleHandleW(nullptr);
    inputState_ = &inputState;

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = WindowClassName;

    if (RegisterClassExW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        throw std::runtime_error("RegisterClassExW failed");
    }

    RECT windowRectangle{
        0,
        0,
        static_cast<LONG>(config.width),
        static_cast<LONG>(config.height)};
    if (AdjustWindowRectEx(&windowRectangle, WS_OVERLAPPEDWINDOW, FALSE, 0) == FALSE)
    {
        throw std::runtime_error("AdjustWindowRectEx failed");
    }

    window_ = CreateWindowExW(
        0,
        WindowClassName,
        config.title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRectangle.right - windowRectangle.left,
        windowRectangle.bottom - windowRectangle.top,
        nullptr,
        nullptr,
        instance_,
        this);
    if (window_ == nullptr)
    {
        throw std::runtime_error("CreateWindowExW failed");
    }

    if (!config.hidden)
    {
        ShowWindow(window_, SW_SHOWDEFAULT);
        UpdateWindow(window_);
    }
}

bool Window::PumpMessages() const noexcept
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE)
    {
        if (message.message == WM_QUIT)
        {
            return false;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return true;
}

HWND Window::NativeHandle() const noexcept
{
    return window_;
}

LRESULT CALLBACK Window::WindowProcedure(
    const HWND window,
    const UINT message,
    const WPARAM wParam,
    const LPARAM lParam)
{
    Window* self = nullptr;
    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<Window*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<Window*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    return self == nullptr
        ? DefWindowProcW(window, message, wParam, lParam)
        : self->HandleMessage(window, message, wParam, lParam);
}

LRESULT Window::HandleMessage(
    const HWND window,
    const UINT message,
    const WPARAM wParam,
    const LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN:
        if (inputState_ != nullptr && wParam <= 0xFFU)
        {
            inputState_->SetKey(static_cast<std::uint8_t>(wParam), true);
        }
        return 0;
    case WM_KEYUP:
        if (inputState_ != nullptr && wParam <= 0xFFU)
        {
            inputState_->SetKey(static_cast<std::uint8_t>(wParam), false);
        }
        return 0;
    case WM_SYSKEYDOWN:
        if (inputState_ != nullptr && wParam <= 0xFFU)
        {
            inputState_->SetKey(static_cast<std::uint8_t>(wParam), true);
        }
        if (wParam == VK_F4 && (lParam & (static_cast<LPARAM>(1) << 29)) != 0)
        {
            SendMessageW(window, WM_CLOSE, 0, 0);
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    case WM_SYSKEYUP:
        if (inputState_ != nullptr && wParam <= 0xFFU)
        {
            inputState_->SetKey(static_cast<std::uint8_t>(wParam), false);
        }
        return DefWindowProcW(window, message, wParam, lParam);
    case WM_KILLFOCUS:
        if (inputState_ != nullptr)
        {
            inputState_->ReleaseAll();
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_NCDESTROY:
        window_ = nullptr;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        return DefWindowProcW(window, message, wParam, lParam);
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}
} // namespace dxa::engine
