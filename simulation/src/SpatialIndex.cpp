#include <dxa/simulation/SpatialIndex.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace dxa::simulation
{
namespace
{
[[nodiscard]] std::vector<SpatialEntity> ValidateAndSortEntities(
    std::vector<SpatialEntity> entities)
{
    if (entities.size() > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::invalid_argument{"Spatial index contains too many entities"};
    }
    std::ranges::sort(entities, {}, &SpatialEntity::id);
    const auto duplicate = std::adjacent_find(
        entities.begin(),
        entities.end(),
        [](const SpatialEntity& left, const SpatialEntity& right) {
            return left.id == right.id;
        });
    if (duplicate != entities.end())
    {
        throw std::invalid_argument{"Spatial entity IDs must be unique"};
    }
    return entities;
}

[[nodiscard]] Aabb2 MakeLooseBounds(
    const Aabb2& tightBounds,
    const float looseness)
{
    const Vec2 minimum = tightBounds.Minimum();
    const Vec2 maximum = tightBounds.Maximum();
    const Vec2 center = tightBounds.Center();
    const Vec2 halfExtent = (maximum - minimum) * (0.5F * looseness);
    return Aabb2::Create(center - halfExtent, center + halfExtent);
}

void SortAndDeduplicate(std::vector<SpatialEntityId>& ids)
{
    std::ranges::sort(ids);
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}
} // namespace

struct LooseQuadtree::Node
{
    Node(const Aabb2& bounds, const float looseness, const std::uint32_t level)
        : tightBounds{bounds},
          looseBounds{MakeLooseBounds(bounds, looseness)},
          depth{level}
    {
    }

    Aabb2 tightBounds;
    Aabb2 looseBounds;
    std::uint32_t depth = 0;
    std::vector<std::size_t> entityIndexes;
    std::array<std::unique_ptr<Node>, 4> children;
};

LinearSpatialIndex::LinearSpatialIndex(std::vector<SpatialEntity> entities)
    : entities_{ValidateAndSortEntities(std::move(entities))}
{
}

SpatialQueryResult LinearSpatialIndex::QueryAabb(const Aabb2& bounds) const
{
    SpatialQueryResult result;
    for (const SpatialEntity& entity : entities_)
    {
        ++result.boundsTested;
        if (entity.bounds.Intersects(bounds))
        {
            result.ids.push_back(entity.id);
        }
    }
    return result;
}

SpatialQueryResult LinearSpatialIndex::PickPoint(const Vec2 point) const
{
    if (!IsFinite(point))
    {
        throw std::invalid_argument{"Spatial point query must be finite"};
    }

    SpatialQueryResult result;
    for (const SpatialEntity& entity : entities_)
    {
        ++result.boundsTested;
        if (entity.bounds.Contains(point))
        {
            result.ids.push_back(entity.id);
        }
    }
    return result;
}

LooseQuadtree::LooseQuadtree(
    const Aabb2 worldBounds,
    std::vector<SpatialEntity> entities,
    const LooseQuadtreeConfig config)
    : entities_{ValidateAndSortEntities(std::move(entities))},
      config_{config}
{
    const Vec2 worldExtent = worldBounds.Maximum() - worldBounds.Minimum();
    if (!std::isfinite(config_.looseness)
        || config_.looseness < 1.0F
        || config_.nodeCapacity == 0
        || config_.maximumDepth == 0
        || worldExtent.x <= 0.0F
        || worldExtent.z <= 0.0F)
    {
        throw std::invalid_argument{"LooseQuadtree configuration is invalid"};
    }
    if (!std::ranges::all_of(
            entities_,
            [&worldBounds](const SpatialEntity& entity) {
                return worldBounds.Contains(entity.bounds);
            }))
    {
        throw std::invalid_argument{"Spatial entity is outside the quadtree world"};
    }

    root_ = std::make_unique<Node>(worldBounds, config_.looseness, 0U);
    for (std::size_t index = 0; index < entities_.size(); ++index)
    {
        Insert(*root_, index);
    }
}

LooseQuadtree::~LooseQuadtree() = default;
LooseQuadtree::LooseQuadtree(LooseQuadtree&&) noexcept = default;
LooseQuadtree& LooseQuadtree::operator=(LooseQuadtree&&) noexcept = default;

void LooseQuadtree::Insert(Node& node, const std::size_t entityIndex)
{
    if (node.children[0] != nullptr)
    {
        const auto child = ContainingChild(node, entities_[entityIndex].bounds);
        if (child.has_value())
        {
            Insert(*node.children[*child], entityIndex);
            return;
        }
    }

    node.entityIndexes.push_back(entityIndex);
    if (node.children[0] == nullptr
        && node.entityIndexes.size() > config_.nodeCapacity
        && node.depth < config_.maximumDepth)
    {
        Split(node);
    }
}

void LooseQuadtree::Split(Node& node)
{
    const Vec2 minimum = node.tightBounds.Minimum();
    const Vec2 maximum = node.tightBounds.Maximum();
    const Vec2 center = node.tightBounds.Center();
    const std::array<Aabb2, 4> childBounds{
        Aabb2::Create(minimum, center),
        Aabb2::Create({center.x, minimum.z}, {maximum.x, center.z}),
        Aabb2::Create({minimum.x, center.z}, {center.x, maximum.z}),
        Aabb2::Create(center, maximum)};
    for (std::size_t index = 0; index < node.children.size(); ++index)
    {
        node.children[index] = std::make_unique<Node>(
            childBounds[index],
            config_.looseness,
            node.depth + 1U);
    }

    std::vector<std::size_t> previousIndexes = std::move(node.entityIndexes);
    node.entityIndexes.clear();
    for (const std::size_t entityIndex : previousIndexes)
    {
        Insert(node, entityIndex);
    }
}

std::optional<std::size_t> LooseQuadtree::ContainingChild(
    const Node& node,
    const Aabb2& entityBounds) const
{
    std::optional<std::size_t> match;
    for (std::size_t index = 0; index < node.children.size(); ++index)
    {
        if (!node.children[index]->looseBounds.Contains(entityBounds))
        {
            continue;
        }
        if (match.has_value())
        {
            return std::nullopt;
        }
        match = index;
    }
    return match;
}

void LooseQuadtree::QueryAabbNode(
    const Node& node,
    const Aabb2& bounds,
    SpatialQueryResult& result) const
{
    if (!node.looseBounds.Intersects(bounds))
    {
        return;
    }

    for (const std::size_t entityIndex : node.entityIndexes)
    {
        ++result.boundsTested;
        const SpatialEntity& entity = entities_[entityIndex];
        if (entity.bounds.Intersects(bounds))
        {
            result.ids.push_back(entity.id);
        }
    }
    for (const auto& child : node.children)
    {
        if (child != nullptr)
        {
            QueryAabbNode(*child, bounds, result);
        }
    }
}

void LooseQuadtree::PickPointNode(
    const Node& node,
    const Vec2 point,
    SpatialQueryResult& result) const
{
    if (!node.looseBounds.Contains(point))
    {
        return;
    }

    for (const std::size_t entityIndex : node.entityIndexes)
    {
        ++result.boundsTested;
        const SpatialEntity& entity = entities_[entityIndex];
        if (entity.bounds.Contains(point))
        {
            result.ids.push_back(entity.id);
        }
    }
    for (const auto& child : node.children)
    {
        if (child != nullptr)
        {
            PickPointNode(*child, point, result);
        }
    }
}

SpatialQueryResult LooseQuadtree::QueryAabb(const Aabb2& bounds) const
{
    SpatialQueryResult result;
    QueryAabbNode(*root_, bounds, result);
    SortAndDeduplicate(result.ids);
    return result;
}

SpatialQueryResult LooseQuadtree::PickPoint(const Vec2 point) const
{
    if (!IsFinite(point))
    {
        throw std::invalid_argument{"Spatial point query must be finite"};
    }

    SpatialQueryResult result;
    PickPointNode(*root_, point, result);
    SortAndDeduplicate(result.ids);
    return result;
}
} // namespace dxa::simulation
