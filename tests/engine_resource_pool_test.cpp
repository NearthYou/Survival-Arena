#include <dxa/engine/ResourcePool.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

namespace
{
using dxa::engine::ResourcePool;

class ThrowingResource
{
public:
    explicit ThrowingResource(const int value)
        : value_(value)
    {
        if (failConstruction)
        {
            throw std::runtime_error("construction failed");
        }
    }

    [[nodiscard]] int Value() const noexcept
    {
        return value_;
    }

    static inline bool failConstruction = false;

private:
    int value_;
};

class NonMovableResource
{
public:
    explicit NonMovableResource(const int value) noexcept
        : value_(value)
    {
    }

    NonMovableResource(const NonMovableResource&) = delete;
    NonMovableResource& operator=(const NonMovableResource&) = delete;
    NonMovableResource(NonMovableResource&&) = delete;
    NonMovableResource& operator=(NonMovableResource&&) = delete;

    [[nodiscard]] int Value() const noexcept
    {
        return value_;
    }

private:
    int value_;
};

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

TEST(ResourcePool, ReclaimsNewSlotWhenConstructionThrows)
{
    ResourcePool<ThrowingResource> pool;
    ThrowingResource::failConstruction = true;
    EXPECT_THROW((void)pool.Create(1), std::runtime_error);

    ThrowingResource::failConstruction = false;
    const auto handle = pool.Create(2);

    EXPECT_EQ(0U, handle.Index());
    ASSERT_NE(nullptr, pool.Get(handle));
    EXPECT_EQ(2, pool.Get(handle)->Value());
    EXPECT_EQ(1U, pool.LiveCount());
}

TEST(ResourcePool, KeepsReusedSlotAvailableWhenConstructionThrows)
{
    ResourcePool<ThrowingResource> pool;
    const auto original = pool.Create(1);
    ASSERT_TRUE(pool.Destroy(original));

    ThrowingResource::failConstruction = true;
    EXPECT_THROW((void)pool.Create(2), std::runtime_error);

    ThrowingResource::failConstruction = false;
    const auto recovered = pool.Create(3);

    EXPECT_EQ(original.Index(), recovered.Index());
    EXPECT_EQ(1U, pool.LiveCount());
}

TEST(ResourcePool, StoresMultipleNonMovableResources)
{
    ResourcePool<NonMovableResource> pool;

    const auto first = pool.Create(10);
    const auto second = pool.Create(20);

    ASSERT_NE(nullptr, pool.Get(first));
    ASSERT_NE(nullptr, pool.Get(second));
    EXPECT_EQ(10, pool.Get(first)->Value());
    EXPECT_EQ(20, pool.Get(second)->Value());
}
} // namespace
