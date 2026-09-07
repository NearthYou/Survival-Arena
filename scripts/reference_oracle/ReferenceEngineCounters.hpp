#pragma once
#include <cstdint>

struct ReferenceDrawCounters
{
    std::uint64_t calls = 0;
    std::uint64_t instances = 0;
    std::uint64_t elements = 0;
};

inline ReferenceDrawCounters& ReferenceDraws()
{
    static ReferenceDrawCounters counters;
    return counters;
}

inline void ReferenceCountDraw(unsigned elements, unsigned instances = 1)
{
    auto& counters = ReferenceDraws();
    ++counters.calls;
    counters.instances += instances;
    counters.elements += static_cast<std::uint64_t>(elements) * instances;
}
