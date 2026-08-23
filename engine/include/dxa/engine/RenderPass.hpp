#pragma once

namespace dxa::engine
{
enum class RenderPass
{
    Forward,
    Shadow,
    GBuffer,
    DeferredLighting,
    Transparent
};
} // namespace dxa::engine
