#pragma once
#include <array>

namespace ArenaAppearance
{
inline Vec4 Tint(int index)
{
    static const std::array<Vec4, 6> colors = {
        Vec4(0.82f,0.89f,0.9f,1), Vec4(0.76f,0.88f,0.68f,1),
        Vec4(1.f,0.73f,0.48f,1), Vec4(0.5f,0.78f,0.93f,1),
        Vec4(0.85f,0.65f,0.87f,1), Vec4(1.f,0.93f,0.76f,1)
    };
    return colors[static_cast<size_t>(std::clamp(index, 0, 5))];
}

inline const wchar_t* Name(int index)
{
    static const wchar_t* names[] = {L"\ud68c\uc0c9",L"\ucd08\ub85d",L"\uc8fc\ud669",L"\ud30c\ub791",L"\ubcf4\ub77c",L"\ud06c\ub9bc"};
    return names[std::clamp(index, 0, 5)];
}
}
