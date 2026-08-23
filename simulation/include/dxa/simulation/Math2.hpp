#pragma once

namespace dxa::simulation
{
struct Vec2
{
    float x = 0.0F;
    float z = 0.0F;

    [[nodiscard]] bool operator==(const Vec2&) const = default;
};

[[nodiscard]] bool IsFinite(Vec2 value) noexcept;
[[nodiscard]] Vec2 operator+(Vec2 left, Vec2 right) noexcept;
[[nodiscard]] Vec2 operator-(Vec2 left, Vec2 right) noexcept;
[[nodiscard]] Vec2 operator*(Vec2 value, float scalar) noexcept;
[[nodiscard]] Vec2 operator/(Vec2 value, float scalar);
[[nodiscard]] float Dot(Vec2 left, Vec2 right) noexcept;
[[nodiscard]] float LengthSquared(Vec2 value) noexcept;
[[nodiscard]] float Length(Vec2 value);
[[nodiscard]] float Distance(Vec2 left, Vec2 right);
[[nodiscard]] Vec2 Normalize(Vec2 value);

class Aabb2
{
public:
    [[nodiscard]] static Aabb2 Create(Vec2 minimum, Vec2 maximum);

    [[nodiscard]] bool Contains(Vec2 point) const noexcept;
    [[nodiscard]] bool Contains(const Aabb2& other) const noexcept;
    [[nodiscard]] bool Intersects(const Aabb2& other) const noexcept;
    [[nodiscard]] Vec2 Minimum() const noexcept;
    [[nodiscard]] Vec2 Maximum() const noexcept;
    [[nodiscard]] Vec2 Center() const noexcept;

private:
    Aabb2(Vec2 minimum, Vec2 maximum) noexcept;

    Vec2 minimum_;
    Vec2 maximum_;
};
} // namespace dxa::simulation
