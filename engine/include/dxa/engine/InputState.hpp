#pragma once

#include <array>
#include <cstdint>

namespace dxa::engine
{
struct PointerPosition
{
    std::int32_t x = 0;
    std::int32_t y = 0;

    [[nodiscard]] bool operator==(const PointerPosition&) const = default;
};

class InputState
{
public:
    void BeginFrame() noexcept
    {
        pressed_.fill(false);
        released_.fill(false);
        rightPointerPressed_ = false;
        rightPointerReleased_ = false;
    }

    void SetKey(const std::uint8_t key, const bool down) noexcept
    {
        if (current_[key] == down)
        {
            return;
        }

        current_[key] = down;
        if (down)
        {
            pressed_[key] = true;
        }
        else
        {
            released_[key] = true;
        }
    }

    void ReleaseAll() noexcept
    {
        for (std::size_t index = 0; index < current_.size(); ++index)
        {
            if (current_[index])
            {
                current_[index] = false;
                released_[index] = true;
            }
        }
        if (rightPointerDown_)
        {
            rightPointerDown_ = false;
            rightPointerReleased_ = true;
        }
    }

    void SetPointerPosition(const std::int32_t x, const std::int32_t y) noexcept
    {
        pointer_ = PointerPosition{x, y};
    }

    void SetRightPointerButton(const bool down) noexcept
    {
        if (rightPointerDown_ == down)
        {
            return;
        }
        rightPointerDown_ = down;
        if (down)
        {
            rightPointerPressed_ = true;
        }
        else
        {
            rightPointerReleased_ = true;
        }
    }

    [[nodiscard]] bool IsDown(const std::uint8_t key) const noexcept
    {
        return current_[key];
    }

    [[nodiscard]] bool WasPressed(const std::uint8_t key) const noexcept
    {
        return pressed_[key];
    }

    [[nodiscard]] bool WasReleased(const std::uint8_t key) const noexcept
    {
        return released_[key];
    }

    [[nodiscard]] PointerPosition Pointer() const noexcept
    {
        return pointer_;
    }

    [[nodiscard]] bool IsRightPointerDown() const noexcept
    {
        return rightPointerDown_;
    }

    [[nodiscard]] bool WasRightPointerPressed() const noexcept
    {
        return rightPointerPressed_;
    }

    [[nodiscard]] bool WasRightPointerReleased() const noexcept
    {
        return rightPointerReleased_;
    }

private:
    std::array<bool, 256> current_{};
    std::array<bool, 256> pressed_{};
    std::array<bool, 256> released_{};
    PointerPosition pointer_;
    bool rightPointerDown_ = false;
    bool rightPointerPressed_ = false;
    bool rightPointerReleased_ = false;
};
} // namespace dxa::engine
