#include <dxa/engine/ResourcePool.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace
{
using dxa::engine::ResourcePool;

TEST(ResourcePool, ReturnsCreatedResource)
{
    ResourcePool<std::string> pool;

    const auto handle = pool.Create("mesh/cube");

    ASSERT_NE(nullptr, pool.Get(handle));
    EXPECT_EQ("mesh/cube", *pool.Get(handle));
    EXPECT_EQ(1U, pool.LiveCount());
}

TEST(ResourcePool, RejectsHandleAfterSlotIsReused)
{
    ResourcePool<std::string> pool;
    const auto stale = pool.Create("old");
    ASSERT_TRUE(pool.Destroy(stale));

    const auto current = pool.Create("current");

    EXPECT_EQ(stale.Index(), current.Index());
    EXPECT_NE(stale.Generation(), current.Generation());
    EXPECT_EQ(nullptr, pool.Get(stale));
    ASSERT_NE(nullptr, pool.Get(current));
    EXPECT_EQ("current", *pool.Get(current));
}

TEST(ResourcePool, RejectsDuplicateDestroy)
{
    ResourcePool<int> pool;
    const auto handle = pool.Create(7);

    EXPECT_TRUE(pool.Destroy(handle));
    EXPECT_FALSE(pool.Destroy(handle));
    EXPECT_EQ(0U, pool.LiveCount());
}

TEST(ResourcePool, StoresMoveOnlyResource)
{
    ResourcePool<std::unique_ptr<int>> pool;

    const auto handle = pool.Create(std::make_unique<int>(42));

    ASSERT_NE(nullptr, pool.Get(handle));
    ASSERT_NE(nullptr, *pool.Get(handle));
    EXPECT_EQ(42, **pool.Get(handle));
}
} // namespace

