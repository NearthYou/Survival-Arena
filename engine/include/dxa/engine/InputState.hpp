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
        previous_ = current_;
    }

    void SetKey(const std::uint8_t key, const bool down) noexcept
    {
        current_[key] = down;
    }

    [[nodiscard]] bool IsDown(const std::uint8_t key) const noexcept
    {
        return current_[key];
    }

    [[nodiscard]] bool WasPressed(const std::uint8_t key) const noexcept
    {
        return current_[key] && !previous_[key];
    }

    [[nodiscard]] bool WasReleased(const std::uint8_t key) const noexcept
    {
        return !current_[key] && previous_[key];
    }

private:
    std::array<bool, 256> current_{};
    std::array<bool, 256> previous_{};
};
} // namespace dxa::engine
