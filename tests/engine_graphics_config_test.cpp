#include <dxa/engine/GraphicsDevice.hpp>

#include <gtest/gtest.h>

namespace
{
TEST(GraphicsDeviceConfig, DefaultsToHardwareDriver)
{
    const dxa::engine::GraphicsDeviceConfig config{};

    EXPECT_EQ(dxa::engine::GraphicsDriver::Hardware, config.driver);
}
} // namespace

