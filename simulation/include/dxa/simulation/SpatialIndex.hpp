#pragma once

#include <dxa/simulation/Math2.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace dxa::simulation
{
using SpatialEntityId = std::uint32_t;

struct SpatialEntity
{
    SpatialEntityId id = 0;
    Aabb2 bounds;
};

struct SpatialQueryResult
{
    std::vector<SpatialEntityId> ids;
    std::uint32_t boundsTested = 0;
};

class LinearSpatialIndex
{
public:
    explicit LinearSpatialIndex(std::vector<SpatialEntity> entities);

    [[nodiscard]] SpatialQueryResult QueryAabb(const Aabb2& bounds) const;
    [[nodiscard]] SpatialQueryResult PickPoint(Vec2 point) const;

private:
    std::vector<SpatialEntity> entities_;
};

struct LooseQuadtreeConfig
{
    float looseness = 1.5F;
    std::uint32_t nodeCapacity = 8;
    std::uint32_t maximumDepth = 6;
};

class LooseQuadtree
{
public:
    LooseQuadtree(
        Aabb2 worldBounds,
        std::vector<SpatialEntity> entities,
        LooseQuadtreeConfig config = {});
    ~LooseQuadtree();

    LooseQuadtree(const LooseQuadtree&) = delete;
    LooseQuadtree& operator=(const LooseQuadtree&) = delete;
    LooseQuadtree(LooseQuadtree&&) noexcept;
    LooseQuadtree& operator=(LooseQuadtree&&) noexcept;

    [[nodiscard]] SpatialQueryResult QueryAabb(const Aabb2& bounds) const;
    [[nodiscard]] SpatialQueryResult PickPoint(Vec2 point) const;

private:
    struct Node;

    void Insert(Node& node, std::size_t entityIndex);
    void Split(Node& node);
    [[nodiscard]] std::optional<std::size_t> ContainingChild(
        const Node& node,
        const Aabb2& entityBounds) const;
    void QueryAabbNode(
        const Node& node,
        const Aabb2& bounds,
        SpatialQueryResult& result) const;
    void PickPointNode(
        const Node& node,
        Vec2 point,
        SpatialQueryResult& result) const;

    std::vector<SpatialEntity> entities_;
    LooseQuadtreeConfig config_;
    std::unique_ptr<Node> root_;
};
} // namespace dxa::simulation
