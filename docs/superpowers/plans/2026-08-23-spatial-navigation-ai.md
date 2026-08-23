# Spatial Navigation and AI Implementation Plan

> For agentic workers: use superpowers:executing-plans to implement this plan task by task. Steps use checkbox syntax for tracking.

Goal: Build the platform-neutral 6주차 simulation module, compare baseline and accelerated spatial queries, connect deterministic NavMesh movement and two AI archetypes, and provide a Windows right-click navigation demo.

Architecture: `dxa_simulation` owns XZ math, immutable NavMesh data, A*, agent movement, broad-phase indexes and AI decisions without Win32 or DirectX dependencies. `dxa_navigation_demo` is the only layer that combines simulation with the DX11 engine. Baseline and accelerated queries share output contracts and benchmark timing is accepted only after exact result equivalence.

Tech Stack: C++20, CMake, GoogleTest, MSVC, GCC 13, DirectX 11 for the Windows demo only

Spec: `docs/superpowers/specs/2026-08-23-spatial-navigation-ai-design.md`

## Global Constraints

- `dxa_simulation` links only the C++ standard library and builds on Windows and Linux.
- Navigation and broad phase use XZ coordinates stored as `Vec2{x, z}`.
- Baseline and accelerated query results are sorted, unique and exactly equal before timing is reported.
- Stable IDs and deterministic tie breaks use input order and ascending numeric ID.
- NavMesh v1 has one flat layer and no funnel, dynamic obstacle or avoidance claim.
- AI returns commands and does not apply animation, damage or world mutations.
- Production behavior is written only after a focused failing test is observed.
- Commits use Korean noun-form Conventional Commit subjects and bodies with 이유, 핵심 변경, 검증.

---

### Task 1: Simulation math and build boundary

Files:
- Create: `simulation/CMakeLists.txt`
- Create: `simulation/include/dxa/simulation/Math2.hpp`
- Create: `simulation/src/Math2.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/simulation_math_test.cpp`

Interfaces:
- Produces: `dxa::simulation::Vec2`
- Produces: `dxa::simulation::Aabb2::Create(Vec2 minimum, Vec2 maximum)`
- Produces: `Length`, `Distance`, `Dot`, `Normalize`, `Contains`, `Intersects`
- Produces: CMake target `dxa_simulation`

- [ ] Step 1: Write failing finite and boundary tests

```cpp
TEST(SimulationMath, IncludesTouchingAabbAndPointBoundaries)
{
    const Aabb2 box = Aabb2::Create({-2.0F, -1.0F}, {2.0F, 3.0F});
    EXPECT_TRUE(box.Contains({2.0F, 3.0F}));
    EXPECT_TRUE(box.Intersects(Aabb2::Create({2.0F, 3.0F}, {4.0F, 5.0F})));
}

TEST(SimulationMath, RejectsNonFiniteAndReversedBounds)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW((void)Aabb2::Create({nan, 0.0F}, {1.0F, 1.0F}), std::invalid_argument);
    EXPECT_THROW((void)Aabb2::Create({2.0F, 0.0F}, {1.0F, 1.0F}), std::invalid_argument);
}
```

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: compile failure because `dxa/simulation/Math2.hpp` and `dxa_simulation` do not exist.

- [ ] Step 3: Add the simulation target and minimal math implementation

```cpp
namespace dxa::simulation
{
struct Vec2
{
    float x = 0.0F;
    float z = 0.0F;
    [[nodiscard]] bool operator==(const Vec2&) const = default;
};

class Aabb2
{
public:
    [[nodiscard]] static Aabb2 Create(Vec2 minimum, Vec2 maximum);
    [[nodiscard]] bool Contains(Vec2 point) const noexcept;
    [[nodiscard]] bool Intersects(const Aabb2& other) const noexcept;
    [[nodiscard]] Vec2 Minimum() const noexcept;
    [[nodiscard]] Vec2 Maximum() const noexcept;
private:
    Vec2 minimum_;
    Vec2 maximum_;
};
}
```

`simulation/CMakeLists.txt` creates a static `dxa_simulation` target, exposes `simulation/include`, and calls `dxa_enable_project_warnings`.

- [ ] Step 4: Run GREEN on Windows and GCC

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^SimulationMath\.' --output-on-failure`

Run: `docker run --rm --mount "type=bind,source=$PWD,target=/src,readonly" gcc:13 g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror -I/src/simulation/include -c /src/simulation/src/Math2.cpp -o /tmp/Math2.o`

Expected: focused tests pass and no warning is emitted.

- [ ] Step 5: Commit

```powershell
git add CMakeLists.txt simulation/CMakeLists.txt simulation/include/dxa/simulation/Math2.hpp simulation/src/Math2.cpp tests/CMakeLists.txt tests/simulation_math_test.cpp
git commit -m "feat(simulation): XZ math 경계 추가"
```

---

### Task 2: NavMesh validation, adjacency and linear point query

Files:
- Create: `simulation/include/dxa/simulation/NavMesh.hpp`
- Create: `simulation/src/NavMesh.cpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/simulation_navmesh_test.cpp`

Interfaces:
- Consumes: `Vec2`, `Aabb2`
- Produces: `TriangleId`, `NavTriangleIndices`, `NavQueryResult`
- Produces: `NavMesh::Build`, `FindContainingTriangleLinear`, `Neighbors`, `TriangleCenter`

- [ ] Step 1: Write failing validation and adjacency tests

```cpp
TEST(NavMesh, BuildsStableAdjacencyFromSharedVertexIndices)
{
    const NavMesh mesh = NavMesh::Build(
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
        {{{0, 1, 2}}, {{1, 3, 2}}});
    EXPECT_EQ(std::vector<TriangleId>{1}, mesh.Neighbors(0));
    EXPECT_EQ(std::vector<TriangleId>{0}, mesh.Neighbors(1));
}

TEST(NavMesh, LinearQueryChoosesLowestTriangleOnSharedEdge)
{
    const NavMesh mesh = MakeTwoTriangleSquare();
    const NavQueryResult result = mesh.FindContainingTriangleLinear({0.5F, 0.5F});
    ASSERT_TRUE(result.triangle.has_value());
    EXPECT_EQ(0U, *result.triangle);
    EXPECT_EQ(2U, result.candidatesTested);
}
```

Add separate tests for an out-of-range index, duplicate triangle vertex, degenerate area, non-finite vertex and an edge shared by three triangles.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: missing `NavMesh.hpp` compile failure.

- [ ] Step 3: Implement immutable validated triangles

```cpp
using TriangleId = std::uint32_t;

struct NavTriangleIndices
{
    std::array<std::uint32_t, 3> vertices{};
};

struct NavQueryResult
{
    std::optional<TriangleId> triangle;
    std::uint32_t candidatesTested = 0;
};

class NavMesh
{
public:
    [[nodiscard]] static NavMesh Build(
        std::vector<Vec2> vertices,
        std::vector<NavTriangleIndices> triangles,
        float gridCellSize = 4.0F);
    [[nodiscard]] NavQueryResult FindContainingTriangleLinear(Vec2 point) const;
    [[nodiscard]] std::span<const TriangleId> Neighbors(TriangleId triangle) const;
    [[nodiscard]] Vec2 TriangleCenter(TriangleId triangle) const;
};
```

Use an ordered edge key `std::pair<std::uint32_t, std::uint32_t>`. Sort every neighbor list after construction. Point-in-triangle accepts edge points with epsilon `1.0e-5F`.

- [ ] Step 4: Run GREEN and full regression

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^NavMesh\.' --output-on-failure`

Run: `./scripts/test.ps1`

Expected: all NavMesh and existing tests pass.

- [ ] Step 5: Commit

```powershell
git add simulation/CMakeLists.txt simulation/include/dxa/simulation/NavMesh.hpp simulation/src/NavMesh.cpp tests/CMakeLists.txt tests/simulation_navmesh_test.cpp
git commit -m "feat(navigation): NavMesh adjacency와 linear query 추가"
```

---

### Task 3: Spatial grid point query equivalence

Files:
- Modify: `simulation/include/dxa/simulation/NavMesh.hpp`
- Create: `simulation/src/NavMeshGrid.cpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `tests/simulation_navmesh_test.cpp`

Interfaces:
- Consumes: validated NavMesh triangle AABBs from Task 2
- Produces: `NavMesh::FindContainingTriangleGrid(Vec2 point) const`
- Produces: deterministic candidate ordering and `candidatesTested`

- [ ] Step 1: Write failing grid equivalence tests

```cpp
TEST(NavMesh, GridMatchesLinearForDeterministicBoundaryQueries)
{
    const NavMesh mesh = MakeGridNavMesh(16, 16, 1.0F, 4.0F);
    std::mt19937 random{20260823U};
    for (std::uint32_t index = 0; index < 100000; ++index)
    {
        const Vec2 point = NextQueryPoint(random);
        EXPECT_EQ(
            mesh.FindContainingTriangleLinear(point).triangle,
            mesh.FindContainingTriangleGrid(point).triangle);
    }
}
```

Add explicit negative-cell, cell-boundary, outside-mesh and shared-edge cases. Assert the median grid candidate count is below total triangle count without asserting wall-clock timing.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: `FindContainingTriangleGrid` is not declared.

- [ ] Step 3: Implement sparse cell indexing

Use `std::int32_t cellX = floor(point.x / gridCellSize)` and the same rule for z. Store a key struct with equality and hash instead of shifting signed values into an unsigned integer.

For each triangle AABB, enumerate inclusive cell minimum to maximum. Sort and erase duplicate triangle IDs in every cell after build. Query increments `candidatesTested` for every exact point-in-triangle test and returns the smallest matching ID.

- [ ] Step 4: Run GREEN and Linux build

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^NavMesh\.' --output-on-failure`

Run: `docker run --rm --mount "type=bind,source=$PWD,target=/src,readonly" gcc:13 sh -lc "g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror -I/src/simulation/include -c /src/simulation/src/NavMesh.cpp -o /tmp/NavMesh.o && g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror -I/src/simulation/include -c /src/simulation/src/NavMeshGrid.cpp -o /tmp/NavMeshGrid.o"`

Expected: 100,000 query results match and GCC emits no warnings.

- [ ] Step 5: Commit

```powershell
git add simulation/CMakeLists.txt simulation/include/dxa/simulation/NavMesh.hpp simulation/src/NavMeshGrid.cpp tests/simulation_navmesh_test.cpp
git commit -m "feat(navigation): spatial grid point query 추가"
```

---

### Task 4: Deterministic A* and NavAgent movement

Files:
- Modify: `simulation/include/dxa/simulation/NavMesh.hpp`
- Create: `simulation/src/NavMeshPath.cpp`
- Create: `simulation/include/dxa/simulation/NavAgent.hpp`
- Create: `simulation/src/NavAgent.cpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/simulation_nav_agent_test.cpp`

Interfaces:
- Produces: `NavPath`, `NavMesh::FindPath(Vec2 start, Vec2 destination)`
- Produces: `NavAgentState`, `NavAgent::SetDestination`, `NavAgent::Tick`

- [ ] Step 1: Write failing path and movement tests

```cpp
TEST(NavMeshPath, ChoosesDeterministicShortestTriangleRoute)
{
    const NavMesh mesh = MakeForkedNavMesh();
    const auto path = mesh.FindPath({0.1F, 0.1F}, {2.9F, 0.9F});
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(std::vector<TriangleId>({0, 1, 3}), path->triangles);
}

TEST(NavAgent, ConsumesLargeDeltaAcrossMultipleWaypointsWithoutOvershoot)
{
    NavAgent agent{mesh, {0.1F, 0.1F}, 4.0F, 0.01F};
    ASSERT_TRUE(agent.SetDestination({2.9F, 0.9F}));
    agent.Tick(1.0F);
    EXPECT_EQ(NavAgentState::Arrived, agent.State());
    EXPECT_NEAR(2.9F, agent.Position().x, 1.0e-4F);
}
```

Add disconnected destination, start outside mesh, same-triangle path, zero delta and negative delta tests.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: `FindPath` and `NavAgent` are missing.

- [ ] Step 3: Implement deterministic A*

```cpp
struct NavPath
{
    std::vector<TriangleId> triangles;
    std::vector<Vec2> waypoints;
    std::uint32_t expandedNodes = 0;
};

[[nodiscard]] std::optional<NavPath> FindPath(
    Vec2 start,
    Vec2 destination) const;
```

Store best g cost and parent for every triangle. Priority order is f cost, h cost, triangle ID. Ignore stale queue entries whose g cost no longer equals the best value. Reconstruct triangle IDs backward, reverse them, then emit start, intermediate centers excluding first and last triangle, and destination.

- [ ] Step 4: Implement NavAgent segment consumption

```cpp
enum class NavAgentState { Idle, Moving, Arrived, InvalidDestination };

class NavAgent
{
public:
    NavAgent(const NavMesh& mesh, Vec2 position, float speed, float stoppingDistance);
    [[nodiscard]] bool SetDestination(Vec2 destination);
    void Tick(float deltaSeconds);
    [[nodiscard]] Vec2 Position() const noexcept;
    [[nodiscard]] NavAgentState State() const noexcept;
};
```

Reject non-finite or negative delta. A zero delta is a no-op. Consume `speed * deltaSeconds` in a loop, snap to a waypoint only when remaining distance reaches it, and continue with leftover distance.

- [ ] Step 5: Run GREEN and full suite

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^(NavMeshPath|NavAgent)\.' --output-on-failure`

Run: `./scripts/test.ps1`

Expected: deterministic path and large-delta tests pass with all regressions green.

- [ ] Step 6: Commit

```powershell
git add simulation/CMakeLists.txt simulation/include/dxa/simulation/NavMesh.hpp simulation/src/NavMeshPath.cpp simulation/include/dxa/simulation/NavAgent.hpp simulation/src/NavAgent.cpp tests/CMakeLists.txt tests/simulation_nav_agent_test.cpp
git commit -m "feat(navigation): deterministic A*와 NavAgent 이동 추가"
```

---

### Task 5: Linear broad phase and loose quadtree

Files:
- Create: `simulation/include/dxa/simulation/SpatialIndex.hpp`
- Create: `simulation/src/SpatialIndex.cpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/simulation_spatial_index_test.cpp`

Interfaces:
- Consumes: `Aabb2`, `Vec2`
- Produces: `SpatialEntity`, `SpatialQueryResult`
- Produces: `LinearSpatialIndex`, `LooseQuadtree`

- [ ] Step 1: Write failing exact-result and validation tests

```cpp
TEST(SpatialIndex, LooseQuadtreeMatchesLinearQueries)
{
    const auto entities = GenerateSpatialEntities(1124, 20260823U);
    const LinearSpatialIndex linear{entities};
    const LooseQuadtree tree{
        Aabb2::Create({-64.0F, -64.0F}, {64.0F, 64.0F}),
        entities,
        {1.5F, 8U, 6U}};
    for (const Aabb2& query : GenerateAabbQueries(20000, 20260823U))
    {
        EXPECT_EQ(linear.QueryAabb(query).ids, tree.QueryAabb(query).ids);
    }
    for (const Vec2 point : GeneratePointQueries(20000, 20260823U))
    {
        EXPECT_EQ(linear.PickPoint(point).ids, tree.PickPoint(point).ids);
    }
}
```

Add duplicate ID, outside-world entity, invalid looseness, zero capacity and zero max depth tests. Add entities on quadrant boundaries and entities too large for a child.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: missing `SpatialIndex.hpp`.

- [ ] Step 3: Implement the shared output contract and linear baseline

```cpp
using SpatialEntityId = std::uint32_t;

struct SpatialEntity { SpatialEntityId id; Aabb2 bounds; };
struct SpatialQueryResult
{
    std::vector<SpatialEntityId> ids;
    std::uint32_t boundsTested = 0;
};
```

The linear index stores entities sorted by ID. Both query methods run exact AABB tests and return sorted IDs.

- [ ] Step 4: Implement loose quadtree insertion and exact query

```cpp
struct LooseQuadtreeConfig
{
    float looseness = 1.5F;
    std::uint32_t nodeCapacity = 8;
    std::uint32_t maximumDepth = 6;
};
```

Each node owns tight bounds, derived loose bounds, local entity indexes and four optional children. Split after capacity when depth permits. Move an entity into a child only when exactly one child loose bounds fully contains it. Query prunes nodes by loose bounds, then performs exact entity bounds checks. Sort and deduplicate final IDs.

- [ ] Step 5: Run GREEN and full suite

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^SpatialIndex\.' --output-on-failure`

Run: `./scripts/test.ps1`

Expected: 40,000 deterministic query results match and all tests pass.

- [ ] Step 6: Commit

```powershell
git add simulation/CMakeLists.txt simulation/include/dxa/simulation/SpatialIndex.hpp simulation/src/SpatialIndex.cpp tests/CMakeLists.txt tests/simulation_spatial_index_test.cpp
git commit -m "feat(spatial): linear과 loose quadtree query 추가"
```

---

### Task 6: Melee and ranged FSM baseline

Files:
- Create: `simulation/include/dxa/simulation/AiDecision.hpp`
- Create: `simulation/src/AiDecision.cpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/simulation_ai_decision_test.cpp`

Interfaces:
- Consumes: `Vec2`
- Produces: `AiArchetype`, `AiCommandType`, `AiBlackboard`, `FsmAiController::Tick`

- [ ] Step 1: Write failing scenario-table tests

```cpp
struct AiScenario
{
    AiBlackboard blackboard;
    AiCommandType expected;
};

TEST(AiFsm, MeleeBaselineChoosesIdleChaseAndAttack)
{
    const FsmAiController controller{AiArchetype::Melee};
    EXPECT_EQ(AiCommandType::Idle, controller.Tick(NoTarget()));
    EXPECT_EQ(AiCommandType::MoveToTarget, controller.Tick(TargetAt(5.0F)));
    EXPECT_EQ(AiCommandType::Attack, controller.Tick(TargetAt(1.0F)));
}

TEST(AiFsm, CooldownBlocksAttackWithoutStoppingChase)
{
    AiBlackboard input = TargetAt(1.0F);
    input.cooldownReady = false;
    EXPECT_EQ(AiCommandType::MoveToTarget, FsmAiController{AiArchetype::Melee}.Tick(input));
}
```

Ranged baseline initially uses Idle, MoveToTarget and Attack only. Validate finite ranges and positions in `Tick`; invalid blackboard throws.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: missing AI types.

- [ ] Step 3: Implement the minimal FSM baseline

```cpp
enum class AiArchetype { Melee, Ranged };
enum class AiCommandType { Idle, MoveToTarget, MoveAwayFromTarget, Attack };

struct AiBlackboard
{
    Vec2 selfPosition;
    Vec2 targetPosition;
    bool hasTarget = false;
    bool cooldownReady = false;
    float attackRange = 1.5F;
    float preferredRange = 8.0F;
    float retreatRange = 3.0F;
};

class FsmAiController
{
public:
    explicit FsmAiController(AiArchetype archetype) noexcept;
    [[nodiscard]] AiCommandType Tick(const AiBlackboard& blackboard) const;
};
```

Order baseline decisions as no target, in attack range with cooldown, otherwise chase. Keep archetype stored even though both baseline branches are equal so Task 7 changes only ranged behavior.

- [ ] Step 4: Run GREEN and commit baseline

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^AiFsm\.' --output-on-failure`

Expected: baseline scenario tests pass.

```powershell
git add simulation/CMakeLists.txt simulation/include/dxa/simulation/AiDecision.hpp simulation/src/AiDecision.cpp tests/CMakeLists.txt tests/simulation_ai_decision_test.cpp
git commit -m "feat(ai): 근접형과 원거리형 FSM 기준 추가"
```

---

### Task 7: Ranged retreat behavior and change-surface evidence

Files:
- Modify: `simulation/src/AiDecision.cpp`
- Modify: `tests/simulation_ai_decision_test.cpp`

Interfaces:
- Consumes: `AiArchetype::Ranged`, `AiBlackboard::retreatRange`
- Produces: `AiCommandType::MoveAwayFromTarget` behavior

- [ ] Step 1: Write failing ranged retreat tests

```cpp
TEST(AiFsm, RangedRetreatsBeforeConsideringAttack)
{
    AiBlackboard input = TargetAt(1.0F);
    input.retreatRange = 3.0F;
    input.attackRange = 10.0F;
    input.cooldownReady = true;
    EXPECT_EQ(
        AiCommandType::MoveAwayFromTarget,
        FsmAiController{AiArchetype::Ranged}.Tick(input));
}
```

Add the boundary case `distance == retreatRange`, which does not retreat and can attack.

- [ ] Step 2: Run RED

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^AiFsm\.' --output-on-failure`

Expected: command is `Attack`, not `MoveAwayFromTarget`.

- [ ] Step 3: Add the ranged-only transition before attack

```cpp
if (archetype_ == AiArchetype::Ranged
    && distanceToTarget < blackboard.retreatRange)
{
    return AiCommandType::MoveAwayFromTarget;
}
```

- [ ] Step 4: Run GREEN and commit the isolated behavior addition

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^AiFsm\.' --output-on-failure`

```powershell
git add simulation/src/AiDecision.cpp tests/simulation_ai_decision_test.cpp
git commit -m "feat(ai): 원거리형 후퇴 전이 추가"
```

- [ ] Step 5: Capture actual change surface without editing history

Run:

```powershell
git diff --numstat HEAD^ HEAD -- simulation/src/AiDecision.cpp tests/simulation_ai_decision_test.cpp
git show --format= --name-only HEAD
```

Expected: output names exactly two changed files. Preserve the numeric output for Task 10 documentation; do not invent a comparison value before this commit exists.

---

### Task 8: Behavior tree core and equivalent AI controllers

Files:
- Create: `simulation/include/dxa/simulation/BehaviorTree.hpp`
- Create: `simulation/src/BehaviorTree.cpp`
- Modify: `simulation/include/dxa/simulation/AiDecision.hpp`
- Modify: `simulation/src/AiDecision.cpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/simulation_behavior_tree_test.cpp`
- Modify: `tests/simulation_ai_decision_test.cpp`

Interfaces:
- Consumes: `AiBlackboard`, `AiCommandType`, `AiArchetype`
- Produces: `BehaviorStatus`, `BehaviorNode`, `ConditionNode`, `ActionNode`, `SequenceNode`, `SelectorNode`
- Produces: `BehaviorTreeAiController::Tick`

- [ ] Step 1: Write failing node semantic tests

```cpp
TEST(BehaviorTree, SelectorStopsAtFirstSuccessfulChild)
{
    std::vector<int> calls;
    SelectorNode selector;
    selector.Add(ActionNode::Create([&](BehaviorContext&) {
        calls.push_back(1); return BehaviorStatus::Failure;
    }));
    selector.Add(ActionNode::Create([&](BehaviorContext&) {
        calls.push_back(2); return BehaviorStatus::Success;
    }));
    selector.Add(ActionNode::Create([&](BehaviorContext&) {
        calls.push_back(3); return BehaviorStatus::Success;
    }));
    EXPECT_EQ(BehaviorStatus::Success, selector.Tick(context));
    EXPECT_EQ(std::vector<int>({1, 2}), calls);
}
```

Add Sequence failure short-circuit, all-success, all-failure and Running propagation tests.

- [ ] Step 2: Run node RED

Run: `./scripts/build.ps1`

Expected: missing BehaviorTree types.

- [ ] Step 3: Implement memoryless synchronous nodes

```cpp
enum class BehaviorStatus { Success, Failure, Running };

struct BehaviorContext
{
    const AiBlackboard* blackboard = nullptr;
    AiCommandType command = AiCommandType::Idle;
};

class BehaviorNode
{
public:
    virtual ~BehaviorNode() = default;
    [[nodiscard]] virtual BehaviorStatus Tick(BehaviorContext& context) const = 0;
};

class ActionNode final : public BehaviorNode
{
public:
    using Function = std::function<BehaviorStatus(BehaviorContext&)>;
    [[nodiscard]] static std::unique_ptr<BehaviorNode> Create(Function function);
    [[nodiscard]] BehaviorStatus Tick(BehaviorContext& context) const override;
};

class BehaviorTreeAiController
{
public:
    explicit BehaviorTreeAiController(AiArchetype archetype);
    [[nodiscard]] AiCommandType Tick(const AiBlackboard& blackboard) const;
private:
    std::unique_ptr<BehaviorNode> root_;
};
```

Composite nodes own `std::vector<std::unique_ptr<BehaviorNode>>`. `Add` accepts a unique pointer and rejects null. Tick methods allocate nothing.

- [ ] Step 4: Write failing FSM and tree equivalence table

```cpp
TEST(AiBehaviorTree, MatchesFsmForMeleeAndRangedScenarios)
{
    for (const AiArchetype archetype : {AiArchetype::Melee, AiArchetype::Ranged})
    {
        const FsmAiController fsm{archetype};
        const BehaviorTreeAiController tree{archetype};
        for (const AiBlackboard& input : AllDecisionScenarios())
        {
            EXPECT_EQ(fsm.Tick(input), tree.Tick(input));
        }
    }
}
```

- [ ] Step 5: Build archetype trees once in the constructor

Melee selector order is attack sequence, chase sequence, idle action. Ranged selector order is retreat sequence, attack sequence, chase sequence, idle action. Every action writes one command and returns Success.

- [ ] Step 6: Run GREEN, allocation regression and full suite

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^(BehaviorTree|AiBehaviorTree|AiFsm)\.' --output-on-failure`

Run: `./scripts/test.ps1`

Expected: node semantics and every FSM/tree command pair match.

- [ ] Step 7: Commit

```powershell
git add simulation/CMakeLists.txt simulation/include/dxa/simulation/BehaviorTree.hpp simulation/src/BehaviorTree.cpp simulation/include/dxa/simulation/AiDecision.hpp simulation/src/AiDecision.cpp tests/CMakeLists.txt tests/simulation_behavior_tree_test.cpp tests/simulation_ai_decision_test.cpp
git commit -m "feat(ai): FSM command를 behavior tree로 전환"
```

---

### Task 9: Windows right-click navigation demo

Files:
- Modify: `engine/include/dxa/engine/InputState.hpp`
- Modify: `engine/src/windows/Window.cpp`
- Modify: `engine/include/dxa/engine/HybridDeferredRenderer.hpp`
- Modify: `engine/src/windows/HybridDeferredRenderer.cpp`
- Create: `apps/navigation_demo/CMakeLists.txt`
- Create: `apps/navigation_demo/src/main.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/engine_input_state_test.cpp`
- Modify: `tests/engine_window_test.cpp`
- Modify: `tests/CMakeLists.txt`

Interfaces:
- Consumes: `NavMesh`, `NavAgent`, engine window, graphics and hybrid renderer
- Produces: pointer position and right button transition in `InputState`
- Produces: `HybridDeferredRenderer::SetControlledPlayerPosition`
- Produces: Windows executable `dxa_navigation_demo`

- [ ] Step 1: Write failing pointer transition tests

```cpp
TEST(InputState, ReportsRightPointerButtonTransitionsAndPosition)
{
    InputState input;
    input.SetPointerPosition(30, 40);
    input.SetRightPointerButton(true);
    EXPECT_EQ(PointerPosition{30, 40}, input.Pointer());
    EXPECT_TRUE(input.WasRightPointerPressed());
    input.BeginFrame();
    EXPECT_FALSE(input.WasRightPointerPressed());
    EXPECT_TRUE(input.IsRightPointerDown());
}
```

Add a Window test that sends `WM_MOUSEMOVE`, `WM_RBUTTONDOWN`, `WM_RBUTTONUP` and checks the same state through the message boundary.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: pointer APIs do not exist.

- [ ] Step 3: Implement pointer capture

Use `GET_X_LPARAM` and `GET_Y_LPARAM` from `windowsx.h`. Update position on move and right-button messages. Release the right button on focus loss through `ReleaseAll`.

- [ ] Step 4: Add controlled-player renderer seam

```cpp
void HybridDeferredRenderer::SetControlledPlayerPosition(
    benchmark::SceneVector3 position)
{
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
        throw std::invalid_argument{"controlled player position must be finite"};
    stressScene_.players.front().position = position;
}
```

Add a WARP test that sets the position, renders two frames and preserves object and pass counts.

- [ ] Step 5: Build the demo loop

The demo constructs a flat 64×64 NavMesh, starts one `NavAgent` at the first player position and samples the same stress camera as the renderer. Convert pointer NDC through inverse view-projection into a ray, intersect y=0, and call `SetDestination` only when `FindContainingTriangleGrid` succeeds.

CLI:

```text
dxa_navigation_demo [--warp] [--hidden] [--frames N]
                    [--auto-destination X Z] [--verify-render]
```

`--hidden` requires a frame limit. `--auto-destination` uses the same agent path as a right click and is permitted only with a frame limit.

Visible interactive mode uses `FrameClock::simulationDeltaSeconds`. Frame-limited `--auto-destination` mode advances the agent by exactly `1.0F / 60.0F` per frame so the smoke result does not depend on machine speed.

- [ ] Step 6: Add WARP navigation smoke

CTest command:

```text
dxa_navigation_demo --warp --hidden --frames 120
  --auto-destination 20 10 --verify-render
```

The process fails unless the agent moved, stayed on NavMesh, rendered a non-clear pixel and finished with no invalid state.

- [ ] Step 7: Run GREEN and commit

Run: `./scripts/build.ps1`

Run: `./scripts/test.ps1`

Expected: pointer tests, demo smoke and all existing tests pass.

```powershell
git add CMakeLists.txt apps/navigation_demo/CMakeLists.txt apps/navigation_demo/src/main.cpp engine/include/dxa/engine/InputState.hpp engine/src/windows/Window.cpp engine/include/dxa/engine/HybridDeferredRenderer.hpp engine/src/windows/HybridDeferredRenderer.cpp tests/engine_input_state_test.cpp tests/engine_window_test.cpp tests/CMakeLists.txt
git commit -m "feat(client): right-click NavMesh 이동 demo 추가"
```

---

### Task 10: Reproducible simulation benchmark and week 6 records

Files:
- Create: `apps/simulation_benchmark/CMakeLists.txt`
- Create: `apps/simulation_benchmark/src/main.cpp`
- Create: `apps/simulation_benchmark/include/dxa/simulation_benchmark/BenchmarkOptions.hpp`
- Modify: `CMakeLists.txt`
- Create: `scripts/run_simulation_benchmark.ps1`
- Create: `scripts/simulation_benchmark_common.ps1`
- Create: `tests/simulation_benchmark_options_test.cpp`
- Create: `tests/simulation_benchmark_runner_test.ps1`
- Modify: `tests/CMakeLists.txt`
- Create: `docs/adr/0004-spatial-navigation-and-behavior-tree.md`
- Create: `docs/devlog/2026-08-23-spatial-navigation-ai.md`
- Create: `docs/benchmarks/spatial-navigation/<run-id>/`
- Modify: `docs/benchmarks/README.md`
- Modify: `README.md`
- Modify: `docs/PROJECT_PLAN.md`

Interfaces:
- Consumes: all simulation query and AI contracts
- Produces: `dxa_simulation_benchmark`
- Produces: clean-commit runner and immutable JSON raw

- [ ] Step 1: Write failing benchmark option and runner guard tests

```cpp
TEST(SimulationBenchmarkOptions, ParsesLockedDefaultWorkload)
{
    const auto result = ParseSimulationBenchmarkOptions({
        "--output", "run", "--commit-sha", "abc", "--seed", "20260823"});
    ASSERT_TRUE(result.options.has_value());
    EXPECT_EQ(100000U, result.options->navQueryCount);
    EXPECT_EQ(20000U, result.options->aabbQueryCount);
    EXPECT_EQ(20000U, result.options->pickQueryCount);
}
```

PowerShell runner tests reject dirty trees, moved HEAD, existing output directories, non-zero mismatch count, wrong commit SHA and missing result checksum.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: benchmark option header and runner are missing.

- [ ] Step 3: Implement deterministic workload and in-memory validation

Build a 64×64 cell NavMesh with 8,192 triangles. Generate point, AABB and pick query arrays before timing. Run linear and accelerated paths over the same arrays. Accumulate IDs into 64-bit FNV-1a checksums and count mismatches before measuring.

If mismatch is non-zero, return exit code 3 before creating the output directory. Otherwise warm each case once, run five samples and write median milliseconds plus candidate totals.

JSON shape:

```json
{
  "schema_version": 1,
  "seed": 20260823,
  "commit_sha": "...",
  "mismatch_count": 0,
  "cases": {
    "nav_linear": {"median_ms": 0.0, "candidates": 0, "checksum": 0},
    "nav_grid": {"median_ms": 0.0, "candidates": 0, "checksum": 0},
    "spatial_linear_aabb": {"median_ms": 0.0, "bounds_tested": 0, "checksum": 0},
    "spatial_quadtree_aabb": {"median_ms": 0.0, "bounds_tested": 0, "checksum": 0}
  }
}
```

Include point-pick cases and AI FSM/tree command checksums in the real output.

- [ ] Step 4: Run Debug, Release and Linux verification

Run: `./scripts/build.ps1`

Run: `./scripts/test.ps1`

Run: `./scripts/build.ps1 -Preset windows-msvc-release`

Run the Ubuntu CI-equivalent CMake configure and build, or GCC 13 Docker compilation when Docker is available.

Expected: Windows and Linux platform-neutral targets pass.

- [ ] Step 5: Commit benchmark code before measuring

```powershell
git add CMakeLists.txt apps/simulation_benchmark/CMakeLists.txt apps/simulation_benchmark/include/dxa/simulation_benchmark/BenchmarkOptions.hpp apps/simulation_benchmark/src/main.cpp scripts/run_simulation_benchmark.ps1 scripts/simulation_benchmark_common.ps1 tests/simulation_benchmark_options_test.cpp tests/simulation_benchmark_runner_test.ps1 tests/CMakeLists.txt
git commit -m "feat(benchmark): simulation query 비교 runner 추가"
```

Require clean `git status` and capture the full commit SHA.

- [ ] Step 6: Run official Release benchmark

Run:

```powershell
./scripts/run_simulation_benchmark.ps1
```

Expected: mismatch count 0, matching checksums, a new timestamp and commit based directory, and no overwritten raw files.

- [ ] Step 7: Capture FSM retreat change surface

Use the isolated `feat(ai): 원거리형 후퇴 전이 추가` commit SHA from Task 7.

```powershell
git show --numstat --format=fuller <retreat-commit> -- simulation/src/AiDecision.cpp tests/simulation_ai_decision_test.cpp
```

Record the exact added and deleted lines and the two file paths. Do not label behavior tree as smaller unless the code evidence supports that statement.

- [ ] Step 8: Write ADR and devlog from actual output

ADR records why flat immutable NavMesh, spatial grid, loose quadtree and memoryless behavior tree were selected. Devlog order is situation, baseline, equivalence gate, measurement, FSM change surface, behavior tree conversion, remaining limits.

Update project status to 6주차 complete only after raw validation, tests and docs links pass.

- [ ] Step 9: Validate docs and raw, then commit evidence

Verify JSON parse, mismatch 0, query counts, checksum pairs, commit SHA, CPU/compiler fields, links, README commands and absence of unfinished marker text.

```powershell
git add README.md docs/PROJECT_PLAN.md docs/adr/0004-spatial-navigation-and-behavior-tree.md docs/devlog/2026-08-23-spatial-navigation-ai.md docs/benchmarks/README.md docs/benchmarks/spatial-navigation
git commit -m "docs(simulation): 공간 탐색과 AI 비교 원본 기록"
```

- [ ] Step 10: Review, push and open the 6주차 PR

Review from merge-base `1f21cd352352e4abfd04e555792ebc6fbe8b229b`. Apply verified findings with regression tests, run the complete suite, push `feat/spatial-navigation-ai`, open a merge-commit PR and monitor Windows and Ubuntu CI. Do not merge without a new user instruction.
