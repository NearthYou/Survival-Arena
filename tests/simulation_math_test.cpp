#include <dxa/simulation/Math2.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace
{
using dxa::simulation::Aabb2;
using dxa::simulation::Distance;
using dxa::simulation::Normalize;
using dxa::simulation::Vec2;

TEST(SimulationMath, IncludesTouchingAabbAndPointBoundaries)
{
    const Aabb2 box = Aabb2::Create({-2.0F, -1.0F}, {2.0F, 3.0F});

    EXPECT_TRUE(box.Contains({2.0F, 3.0F}));
    EXPECT_TRUE(box.Contains(Aabb2::Create({-1.0F, 0.0F}, {1.0F, 2.0F})));
    EXPECT_TRUE(box.Intersects(Aabb2::Create({2.0F, 3.0F}, {4.0F, 5.0F})));
    EXPECT_FALSE(box.Intersects(Aabb2::Create({2.01F, 3.01F}, {4.0F, 5.0F})));
    EXPECT_EQ((Vec2{0.0F, 1.0F}), box.Center());
}

TEST(SimulationMath, RejectsNonFiniteAndReversedBounds)
{
    const float notANumber = std::numeric_limits<float>::quiet_NaN();

    EXPECT_THROW(
        (void)Aabb2::Create({notANumber, 0.0F}, {1.0F, 1.0F}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)Aabb2::Create({2.0F, 0.0F}, {1.0F, 1.0F}),
        std::invalid_argument);
}

TEST(SimulationMath, NormalizesFiniteVectorAndRejectsZeroLength)
{
    const Vec2 normalized = Normalize({3.0F, 4.0F});

    EXPECT_NEAR(0.6F, normalized.x, 1.0e-6F);
    EXPECT_NEAR(0.8F, normalized.z, 1.0e-6F);
    EXPECT_NEAR(5.0F, Distance({0.0F, 0.0F}, {3.0F, 4.0F}), 1.0e-6F);
    EXPECT_THROW((void)Normalize({0.0F, 0.0F}), std::invalid_argument);
}
} // namespace
