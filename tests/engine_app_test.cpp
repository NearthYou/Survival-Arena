#include <dxa/engine/EngineApp.hpp>

#include <gtest/gtest.h>

#include <Windows.h>

#include <filesystem>
#include <stdexcept>

namespace
{
void DrainThreadMessages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE)
    {
    }
}

TEST(EngineApp, RenderVerificationRejectsQuitBeforeFirstFrame)
{
    DrainThreadMessages();
    PostQuitMessage(0);

    const dxa::engine::EngineRunOptions options{
        320,
        180,
        1,
        true,
        false,
        true,
        dxa::engine::GraphicsDriver::Warp};

    EXPECT_THROW(
        (void)dxa::engine::EngineApp{}.Run(
            options, std::filesystem::path{DXA_TEST_SHADER_PATH}),
        std::runtime_error);

    DrainThreadMessages();
}
} // namespace
