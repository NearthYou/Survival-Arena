#pragma once

#include <array>
#include <cstdint>

namespace dxa::engine
{
class InputState
{
public:
    void BeginFrame() noexcept
    {
        pressed_.fill(false);
        released_.fill(false);
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

private:
    std::array<bool, 256> current_{};
    std::array<bool, 256> pressed_{};
    std::array<bool, 256> released_{};
};
} // namespace dxa::engine
