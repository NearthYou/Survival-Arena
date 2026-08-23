#pragma once

#include <string_view>

namespace dxa::engine
{
enum class RenderPath
{
    Forward,
    HybridDeferred
};

[[nodiscard]] constexpr std::string_view ToString(const RenderPath path) noexcept
{
    return path == RenderPath::Forward ? "forward" : "hybrid-deferred";
}
} // namespace dxa::engine
