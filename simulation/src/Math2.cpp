#include <dxa/simulation/Math2.hpp>

#include <cmath>
#include <stdexcept>

namespace dxa::simulation
{
bool IsFinite(const Vec2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.z);
}

Vec2 operator+(const Vec2 left, const Vec2 right) noexcept
{
    return Vec2{left.x + right.x, left.z + right.z};
}

Vec2 operator-(const Vec2 left, const Vec2 right) noexcept
{
    return Vec2{left.x - right.x, left.z - right.z};
}

Vec2 operator*(const Vec2 value, const float scalar) noexcept
{
    return Vec2{value.x * scalar, value.z * scalar};
}

Vec2 operator/(const Vec2 value, const float scalar)
{
    if (!std::isfinite(scalar) || scalar == 0.0F)
    {
        throw std::invalid_argument{"Vec2 division requires a finite non-zero scalar"};
    }
    return Vec2{value.x / scalar, value.z / scalar};
}

float Dot(const Vec2 left, const Vec2 right) noexcept
{
    return left.x * right.x + left.z * right.z;
}

float LengthSquared(const Vec2 value) noexcept
{
    return Dot(value, value);
}

float Length(const Vec2 value)
{
    const float lengthSquared = LengthSquared(value);
    if (!std::isfinite(lengthSquared))
    {
        throw std::invalid_argument{"Vec2 length requires finite coordinates"};
    }
    return std::sqrt(lengthSquared);
}

float Distance(const Vec2 left, const Vec2 right)
{
    return Length(right - left);
}

Vec2 Normalize(const Vec2 value)
{
    const float length = Length(value);
    if (length <= 0.0F)
    {
        throw std::invalid_argument{"Vec2 normalization requires non-zero length"};
    }
    return value / length;
}

Aabb2 Aabb2::Create(const Vec2 minimum, const Vec2 maximum)
{
    if (!IsFinite(minimum)
        || !IsFinite(maximum)
        || minimum.x > maximum.x
        || minimum.z > maximum.z)
    {
        throw std::invalid_argument{"Aabb2 requires finite ordered bounds"};
    }
    return Aabb2{minimum, maximum};
}

Aabb2::Aabb2(const Vec2 minimum, const Vec2 maximum) noexcept
    : minimum_{minimum}, maximum_{maximum}
{
}

bool Aabb2::Contains(const Vec2 point) const noexcept
{
    return IsFinite(point)
        && point.x >= minimum_.x
        && point.x <= maximum_.x
        && point.z >= minimum_.z
        && point.z <= maximum_.z;
}

bool Aabb2::Contains(const Aabb2& other) const noexcept
{
    return Contains(other.minimum_) && Contains(other.maximum_);
}

bool Aabb2::Intersects(const Aabb2& other) const noexcept
{
    return minimum_.x <= other.maximum_.x
        && maximum_.x >= other.minimum_.x
        && minimum_.z <= other.maximum_.z
        && maximum_.z >= other.minimum_.z;
}

Vec2 Aabb2::Minimum() const noexcept
{
    return minimum_;
}

Vec2 Aabb2::Maximum() const noexcept
{
    return maximum_;
}

Vec2 Aabb2::Center() const noexcept
{
    return Vec2{
        (minimum_.x + maximum_.x) * 0.5F,
        (minimum_.z + maximum_.z) * 0.5F};
}
} // namespace dxa::simulation
