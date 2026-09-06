#pragma once

#include "InputManager.h"
#include <array>
#include <vector>

// In-process test input only. Never reads, moves or injects the desktop cursor.
struct ReferenceProbeInputState
{
    std::array<bool, 256> keys{};
    POINT mouse{};
};

inline ReferenceProbeInputState& ReferenceProbeInput()
{
    static ReferenceProbeInputState input;
    return input;
}

inline void ReferenceApplyProbeInput(std::vector<KEY_STATE>& states, POINT& mouse)
{
    const auto& input = ReferenceProbeInput();
    for (std::size_t key = 0; key < input.keys.size(); ++key)
    {
        const bool wasDown = states[key] == KEY_STATE::DOWN || states[key] == KEY_STATE::PRESS;
        states[key] = input.keys[key]
            ? (wasDown ? KEY_STATE::PRESS : KEY_STATE::DOWN)
            : (wasDown ? KEY_STATE::UP : KEY_STATE::NONE);
    }
    mouse = input.mouse;
}
