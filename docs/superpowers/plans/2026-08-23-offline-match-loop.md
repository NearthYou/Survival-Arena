# Offline Match Loop Implementation Plan

> For agentic workers: REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking. This repository does not use subagents for this milestone.

Goal: Build one deterministic offline survival-arena match with 24 contenders, 100 neutral actors, three weapons, loot, a four-stage safe zone, death, and a final winner, then render and verify it through DX11.

Architecture: `dxa_simulation` owns an authoritative `OfflineMatch` aggregate that advances only through 30Hz `Step()` calls. Commands enter before a tick; stable events and a read-only snapshot leave after it. A Windows app adapter converts snapshots into generic engine character state, so the renderer never includes simulation types and the same match code can move to the game server in week 9.

Tech Stack: C++20, CMake, GoogleTest, existing NavMesh and NavAgent, existing BehaviorTree AI, DirectX 11 WARP, PowerShell evidence runners.

Spec: `docs/superpowers/specs/2026-08-23-offline-match-loop-design.md`

## Global Constraints

- `simulation` must compile on Windows MSVC and Linux GCC without Win32 or DirectX headers.
- Match time advances only by one fixed 30Hz tick per `OfflineMatch::Step()` call.
- The canonical population is exactly 24 contenders and 100 neutral actors.
- Neutral population is exactly 50 melee and 50 ranged actors.
- Weapons are Blade, Rifle, and ArcPulse with integer cooldown ticks.
- Canonical safe-zone stages end at ticks 3,600, 7,200, 10,800, 14,400, and hard timeout 18,000.
- Canonical seed is `20260823`.
- Canonical match result must be between ticks 14,400 and 18,000 inclusive.
- All actor, loot, command, event, snapshot, and checksum ordering is stable by numeric ID.
- Baseline navigation, renderer stress-scene behavior, and week 6 benchmark raw files remain unchanged.
- No ammo, armor, crafting, persistence, networking, projectile physics, body blocking, or multiple inventory slots.
- Korean noun-form Conventional Commit subjects are used. Commit bodies record 이유, 핵심 변경, 검증.
- Branch and commit names do not contain automation tool names.
- No benchmark raw file is overwritten.

---

## File Map

### Simulation public contracts

- `simulation/include/dxa/simulation/MatchTypes.hpp`: IDs, enums, command, event, actor snapshot, match snapshot, result.
- `simulation/include/dxa/simulation/MatchConfig.hpp`: locked defaults and config validation.
- `simulation/include/dxa/simulation/Combat.hpp`: weapon catalog, combat actor state, attack intent, batch damage result.
- `simulation/include/dxa/simulation/SafeZone.hpp`: safe-zone schedule and tick evaluation.
- `simulation/include/dxa/simulation/Loot.hpp`: loot item state and one-pickup resolution.
- `simulation/include/dxa/simulation/MatchResolution.hpp`: deterministic survivor ranking input and winner selection.
- `simulation/include/dxa/simulation/OfflineBotController.hpp`: deterministic contender and neutral decision input/output.
- `simulation/include/dxa/simulation/OfflineMatch.hpp`: deep public match interface with pimpl storage.

### Simulation implementation

- `simulation/src/MatchConfig.cpp`: value and population validation.
- `simulation/src/Combat.cpp`: cooldown and simultaneous damage resolution.
- `simulation/src/SafeZone.cpp`: phase interpolation and whole-second damage schedule.
- `simulation/src/Loot.cpp`: deterministic nearest-ID pickup and item application.
- `simulation/src/MatchResolution.cpp`: alive, health, eliminations, and ID ranking.
- `simulation/src/OfflineMatchInternal.hpp`: private actor state, deterministic RNG, event hashing, NavAgent ownership.
- `simulation/src/OfflineMatch.cpp`: lifecycle, command queue, snapshot, event drain.
- `simulation/src/OfflineMatchSpawn.cpp`: participant, neutral, and loot spawning.
- `simulation/src/OfflineMatchStep.cpp`: tick ordering and match result.
- `simulation/src/OfflineMatchBots.cpp`: contender and neutral decisions at 5Hz.

### Windows integration

- `cmake/DeployHybridRuntime.cmake`: one shared shader and runtime-asset deployment function for both DX11 demos.
- `engine/include/dxa/engine/HybridDeferredRenderer.hpp`: generic character state batch and zone-radius override.
- `engine/src/windows/HybridDeferredRenderer.cpp`: update stress-scene actor slots without simulation dependency.
- `engine/include/dxa/engine/Window.hpp`: title update seam.
- `engine/src/windows/Window.cpp`: validated `SetTitle` implementation.
- `apps/offline_match_demo/CMakeLists.txt`: executable, assets, shaders, WARP test.
- `apps/offline_match_demo/src/main.cpp`: CLI, fixed-step loop, command adapter, snapshot-to-render adapter.

### Measurement and records

- `apps/offline_match_benchmark/CMakeLists.txt`: platform-neutral Release benchmark target.
- `apps/offline_match_benchmark/include/dxa/offline_match_benchmark/BenchmarkOptions.hpp`: output, SHA, seed parser.
- `apps/offline_match_benchmark/src/main.cpp`: canonical match timing and immutable JSON/CSV.
- `scripts/run_offline_match_benchmark.ps1`: clean-commit Release runner.
- `scripts/offline_match_benchmark_common.ps1`: JSON and CSV evidence validation.
- `docs/adr/0005-authoritative-offline-match.md`: match ownership and tick-order decision.
- `docs/devlog/2026-08-24-offline-match-loop.md`: actual symptoms, decisions, measurements, limits.

### Tests

- `tests/simulation_match_types_test.cpp`
- `tests/simulation_combat_test.cpp`
- `tests/simulation_safe_zone_test.cpp`
- `tests/simulation_loot_test.cpp`
- `tests/simulation_match_resolution_test.cpp`
- `tests/simulation_offline_match_test.cpp`
- `tests/simulation_offline_match_bot_test.cpp`
- `tests/engine_hybrid_deferred_renderer_test.cpp`
- `tests/engine_window_test.cpp`
- `tests/offline_match_benchmark_options_test.cpp`
- `tests/offline_match_benchmark_runner_test.ps1`

---

### Task 1: Match domain types and locked configuration

Files:

- Create: `simulation/include/dxa/simulation/MatchTypes.hpp`
- Create: `simulation/include/dxa/simulation/MatchConfig.hpp`
- Create: `simulation/src/MatchConfig.cpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/simulation_match_types_test.cpp`

Interfaces:

- Produces: `ActorId`, `LootId`, `ActorRole`, `NeutralArchetype`, `WeaponType`, `LootType`, `MatchPhase`, `SafeZoneStage`, `MatchEndReason`, `MatchEventType`.
- Produces: `MatchCommand`, `MatchEvent`, `ActorSnapshot`, `LootSnapshot`, `MatchResult`, `MatchSnapshot`.
- Produces: `MatchConfig DefaultMatchConfig()` and `void ValidateMatchConfig(const MatchConfig&)`.

- [ ] Step 1: Write the failing domain and config tests

```cpp
TEST(MatchConfig, LocksTheCanonicalPopulationAndTiming)
{
    const MatchConfig config = DefaultMatchConfig();
    EXPECT_EQ(30U, config.tickRate);
    EXPECT_EQ(24U, config.contenderCount);
    EXPECT_EQ(50U, config.meleeNeutralCount);
    EXPECT_EQ(50U, config.rangedNeutralCount);
    EXPECT_EQ(14400U, config.suddenDeathTick);
    EXPECT_EQ(18000U, config.hardTimeoutTick);
    EXPECT_EQ(20260823U, config.seed);
    EXPECT_FLOAT_EQ(128.0F, config.arenaHalfExtent);
    EXPECT_FLOAT_EQ(80.0F, config.contenderSpawnInnerRadius);
    EXPECT_FLOAT_EQ(104.0F, config.contenderSpawnOuterRadius);
}

TEST(MatchConfig, RejectsInvalidPopulationAndTiming)
{
    MatchConfig config = DefaultMatchConfig();
    config.contenderCount = 1U;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);
    config = DefaultMatchConfig();
    config.hardTimeoutTick = config.suddenDeathTick - 1U;
    EXPECT_THROW(ValidateMatchConfig(config), std::invalid_argument);
}
```

Add equality tests for snapshots and verify optional winner and killer IDs distinguish absence from actor zero.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: compilation fails because `dxa/simulation/MatchConfig.hpp` and `MatchTypes.hpp` do not exist.

- [ ] Step 3: Implement the contracts and validation

Use this public shape:

```cpp
using ActorId = std::uint32_t;
using LootId = std::uint32_t;

enum class ActorRole { Contender, Neutral };
enum class NeutralArchetype { None, Melee, Ranged };
enum class WeaponType { Blade, Rifle, ArcPulse };
enum class LootType { Rifle, ArcPulse, MedKit };
enum class MatchPhase { Waiting, Running, SuddenDeath, Finished };
enum class SafeZoneStage { Stage1, Stage2, Stage3, Stage4, SuddenDeath };
enum class MatchEndReason { LastSurvivor, TimeLimit };
enum class MatchEventType {
    CommandRejected,
    LootPickedUp,
    WeaponChanged,
    ActorHealed,
    DamageApplied,
    ActorDied,
    MatchFinished
};

struct MatchCommand
{
    ActorId actor = 0;
    std::optional<Vec2> moveDestination;
    std::optional<ActorId> attackTarget;
};

struct MatchConfig
{
    std::uint32_t tickRate = 30U;
    std::uint32_t contenderCount = 24U;
    std::uint32_t meleeNeutralCount = 50U;
    std::uint32_t rangedNeutralCount = 50U;
    std::uint32_t rifleLootCount = 24U;
    std::uint32_t arcPulseLootCount = 12U;
    std::uint32_t medKitLootCount = 24U;
    std::uint32_t botDecisionIntervalTicks = 6U;
    std::uint32_t maximumSpawnAttempts = 4096U;
    std::uint32_t suddenDeathTick = 14400U;
    std::uint32_t hardTimeoutTick = 18000U;
    std::uint32_t seed = 20260823U;
    float contenderSpeed = 6.0F;
    float neutralSpeed = 4.5F;
    float contenderPerceptionRadius = 18.0F;
    float neutralPerceptionRadius = 10.0F;
    float pickupRadius = 1.0F;
    float contenderSpawnSpacing = 3.0F;
    float neutralSpawnSpacing = 0.75F;
    float arenaHalfExtent = 128.0F;
    float contenderSpawnInnerRadius = 80.0F;
    float contenderSpawnOuterRadius = 104.0F;
};

struct MatchEvent
{
    std::uint32_t tick = 0;
    MatchEventType type = MatchEventType::CommandRejected;
    ActorId actor = 0;
    std::optional<ActorId> subject;
    std::optional<LootId> loot;
    std::int32_t amount = 0;
    std::optional<WeaponType> weapon;
};

struct ActorSnapshot
{
    ActorId id = 0;
    ActorRole role = ActorRole::Contender;
    NeutralArchetype neutralArchetype = NeutralArchetype::None;
    Vec2 position;
    std::int32_t health = 100;
    bool alive = true;
    WeaponType weapon = WeaponType::Blade;
    std::uint32_t cooldownTicksRemaining = 0;
    std::uint32_t eliminations = 0;
};

struct LootSnapshot
{
    LootId id = 0;
    LootType type = LootType::Rifle;
    Vec2 position;
    bool active = true;
};

struct MatchResult
{
    ActorId winner = 0;
    MatchEndReason reason = MatchEndReason::LastSurvivor;
    std::uint32_t finishedTick = 0;
};

struct MatchSnapshot
{
    std::uint32_t tick = 0;
    double elapsedSeconds = 0.0;
    MatchPhase phase = MatchPhase::Waiting;
    SafeZoneStage safeZoneStage = SafeZoneStage::Stage1;
    Vec2 safeZoneCenter;
    float safeZoneRadius = 128.0F;
    std::uint32_t aliveContenders = 0;
    std::vector<ActorSnapshot> actors;
    std::vector<LootSnapshot> loot;
    std::optional<MatchResult> result;
    std::uint64_t eventChecksum = 0;
};
```

Lock event field meaning: `CommandRejected.actor` is the sender; loot, weapon, and heal events use the affected actor; `DamageApplied.actor` is the damaged actor and `subject` is the optional attacker; `ActorDied.actor` is the dead actor and `subject` is the optional killer; `MatchFinished.actor` is the winner. Zone damage and zone death leave `subject` empty. `amount` is positive damage or healing, never a signed delta.

Reject any tick rate other than 30, contender count below 2, decision interval other than 6, zero spawn attempts, sudden death other than 14,400, hard timeout other than 18,000, non-finite or non-positive speed, perception, pickup, spacing, and arena extent values, or any count total that would overflow `ActorId` or `LootId`. Spawn radii must be finite, inner radius must be non-negative, outer radius must be at least the inner radius, and outer radius must not exceed arena half extent. Custom smaller actor counts and smaller valid spawn radii remain available for focused tests; `DefaultMatchConfig` is the canonical 24 plus 100 contract.

- [ ] Step 4: Run GREEN

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^MatchConfig\.' --output-on-failure`

Expected: all `MatchConfig` tests pass.

- [ ] Step 5: Commit

```powershell
git add simulation/CMakeLists.txt simulation/include/dxa/simulation/MatchTypes.hpp simulation/include/dxa/simulation/MatchConfig.hpp simulation/src/MatchConfig.cpp tests/CMakeLists.txt tests/simulation_match_types_test.cpp
git commit -m "feat(match): 오프라인 경기 domain과 config 추가" -m "이유: 이후 경기 규칙이 공유할 ID와 상태, 고정 수치를 먼저 검증하기 위해 추가했습니다." -m "핵심 변경: match type, snapshot, event, canonical config와 유효성 검사를 구현했습니다." -m "검증: Windows Debug build와 MatchConfig focused test 통과 결과를 기록했습니다."
```

---

### Task 2: Weapon catalog and simultaneous combat resolution

Files:

- Create: `simulation/include/dxa/simulation/Combat.hpp`
- Create: `simulation/src/Combat.cpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/simulation_combat_test.cpp`

Interfaces:

- Consumes: match IDs and enums from Task 1.
- Produces: `WeaponDefinition WeaponDefinitionFor(WeaponType)`.
- Produces: `CombatActor`, `AttackIntent`, `DamageRecord`, `DeathRecord`, `CombatResolution`.
- Produces: `void TickWeaponCooldowns(std::span<CombatActor>)`.
- Produces: `CombatResolution ResolveAttacks(std::span<CombatActor>, std::span<const AttackIntent>)`.

- [ ] Step 1: Write failing weapon and damage-batch tests

```cpp
TEST(Combat, LocksWeaponDefinitions)
{
    EXPECT_EQ((WeaponDefinition{2.2F, 0.0F, 24, 21U}),
              WeaponDefinitionFor(WeaponType::Blade));
    EXPECT_EQ((WeaponDefinition{18.0F, 0.0F, 12, 12U}),
              WeaponDefinitionFor(WeaponType::Rifle));
    EXPECT_EQ((WeaponDefinition{10.0F, 5.0F, 18, 90U}),
              WeaponDefinitionFor(WeaponType::ArcPulse));
}

TEST(Combat, ResolvesSameTickDamageWithoutAttackerOrderBias)
{
    auto forward = MakeThreeCombatants();
    auto reverse = forward;
    const std::array forwardIntents{AttackIntent{0, 2}, AttackIntent{1, 2}};
    const std::array reverseIntents{AttackIntent{1, 2}, AttackIntent{0, 2}};
    EXPECT_EQ(
        ResolveAttacks(forward, forwardIntents),
        ResolveAttacks(reverse, reverseIntents));
}
```

Add tests for cooldown rejection, out-of-range rejection, dead actors, neutral-to-neutral rejection, ArcPulse excluding its attacker, simultaneous lethal damage, highest-damage killer attribution, and lower-ID attribution on an equal contribution.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: compilation fails because `Combat.hpp` is missing.

- [ ] Step 3: Implement minimal combat rules

```cpp
struct WeaponDefinition
{
    float range = 0.0F;
    float effectRadius = 0.0F;
    std::int32_t damage = 0;
    std::uint32_t cooldownTicks = 0;
};

struct CombatActor
{
    ActorId id = 0;
    ActorRole role = ActorRole::Contender;
    Vec2 position;
    std::int32_t health = 100;
    bool alive = true;
    WeaponType weapon = WeaponType::Blade;
    std::uint32_t cooldownTicksRemaining = 0;
    std::uint32_t eliminations = 0;
};

struct AttackIntent
{
    ActorId attacker = 0;
    ActorId target = 0;
};

struct DamageRecord
{
    ActorId target = 0;
    std::int32_t amount = 0;
    std::optional<ActorId> primarySource;
};

struct DeathRecord
{
    ActorId victim = 0;
    std::optional<ActorId> killer;
};

struct CombatResolution
{
    std::vector<AttackIntent> acceptedIntents;
    std::vector<DamageRecord> damage;
    std::vector<DeathRecord> deaths;
};
```

Validate all intents against the pre-damage actor state. Sort accepted intents by attacker then target ID. Accumulate damage per target. Apply total damage once. For a lethal target, choose the killer by greatest accepted damage contribution and then smallest attacker ID. Return sorted damage and death records; do not emit match events in this module.

- [ ] Step 4: Run GREEN and regression suite

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^Combat\.' --output-on-failure`

Expected: every combat test passes and no existing test fails to link.

- [ ] Step 5: Commit

```powershell
git add simulation/CMakeLists.txt simulation/include/dxa/simulation/Combat.hpp simulation/src/Combat.cpp tests/CMakeLists.txt tests/simulation_combat_test.cpp
git commit -m "feat(combat): 무기와 동시 피해 판정 추가" -m "이유: attacker 순서가 같은 tick의 생사 결과를 바꾸지 않도록 피해 판정을 분리했습니다." -m "핵심 변경: 무기 수치, cooldown, 단일 및 범위 공격, 동시 피해와 처치자 판정을 구현했습니다." -m "검증: Windows Debug build와 Combat focused test 통과 결과를 기록했습니다."
```

---

### Task 3: Four-stage safe zone and sudden death

Files:

- Create: `simulation/include/dxa/simulation/SafeZone.hpp`
- Create: `simulation/src/SafeZone.cpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/simulation_safe_zone_test.cpp`

Interfaces:

- Consumes: `Vec2` and match tick rate.
- Produces: `SafeZoneState EvaluateSafeZone(std::uint32_t tick, std::uint32_t tickRate)`.
- Produces: `bool IsOutsideSafeZone(Vec2, const SafeZoneState&)`.
- Produces: `std::int32_t SafeZoneDamageForTick(std::uint32_t tick, std::uint32_t tickRate)`.

- [ ] Step 1: Write failing phase-boundary tests

```cpp
TEST(SafeZone, InterpolatesEveryLockedBoundary)
{
    EXPECT_FLOAT_EQ(128.0F, EvaluateSafeZone(0U, 30U).radius);
    EXPECT_FLOAT_EQ(112.0F, EvaluateSafeZone(3600U, 30U).radius);
    EXPECT_FLOAT_EQ(96.0F, EvaluateSafeZone(7200U, 30U).radius);
    EXPECT_FLOAT_EQ(80.0F, EvaluateSafeZone(10800U, 30U).radius);
    EXPECT_FLOAT_EQ(64.0F, EvaluateSafeZone(14400U, 30U).radius);
    EXPECT_FLOAT_EQ(0.0F, EvaluateSafeZone(18000U, 30U).radius);
}

TEST(SafeZone, AppliesIntegerDamageOnlyOnWholeSeconds)
{
    EXPECT_EQ(0, SafeZoneDamageForTick(29U, 30U));
    EXPECT_EQ(2, SafeZoneDamageForTick(30U, 30U));
    EXPECT_EQ(16, SafeZoneDamageForTick(10800U, 30U));
    EXPECT_EQ(32, SafeZoneDamageForTick(14400U, 30U));
}
```

Add boundary inclusion, non-finite position rejection, zero tick-rate rejection, and phase enum tests.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: `SafeZone.hpp` is missing.

- [ ] Step 3: Implement table-driven evaluation

Use a constexpr table of `{startTickAt30Hz, startRadius, endRadius, damagePerSecond, phase}`. Scale only tick boundaries through `tickRate`; do not derive damage from frame delta. Use double for interpolation fraction and return finite float radius.

```cpp
struct SafeZoneState
{
    SafeZoneStage stage = SafeZoneStage::Stage1;
    Vec2 center{0.0F, 0.0F};
    float radius = 128.0F;
    std::int32_t damagePerSecond = 2;
};
```

- [ ] Step 4: Run GREEN

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^SafeZone\.' --output-on-failure`

- [ ] Step 5: Commit

```powershell
git add simulation/CMakeLists.txt simulation/include/dxa/simulation/SafeZone.hpp simulation/src/SafeZone.cpp tests/CMakeLists.txt tests/simulation_safe_zone_test.cpp
git commit -m "feat(zone): 4단계 축소 구역과 sudden death 추가" -m "이유: 경기 시간을 제한하고 위치 선택에 압박을 주는 규칙을 tick 기준으로 고정했습니다." -m "핵심 변경: phase별 반경 보간과 초 단위 zone 피해 계산을 구현했습니다." -m "검증: Windows Debug build와 SafeZone focused test 통과 결과를 기록했습니다."
```

---

### Task 4: Loot pickup and health recovery

Files:

- Create: `simulation/include/dxa/simulation/Loot.hpp`
- Create: `simulation/src/Loot.cpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/simulation_loot_test.cpp`

Interfaces:

- Consumes: `CombatActor`, `LootType`, `WeaponType`.
- Produces: `LootItem` and `LootPickupResult`.
- Produces: `std::optional<LootPickupResult> ResolveNearestLootPickup(CombatActor&, std::span<LootItem>, float pickupRadius)`.

- [ ] Step 1: Write failing deterministic pickup tests

```cpp
TEST(Loot, ChoosesTheLowestIdInsidePickupRadius)
{
    CombatActor actor = AliveContenderAt({0.0F, 0.0F});
    std::array loot{
        LootItem{8U, LootType::Rifle, {0.5F, 0.0F}, true},
        LootItem{3U, LootType::ArcPulse, {0.8F, 0.0F}, true}};
    const auto result = ResolveNearestLootPickup(actor, loot, 1.0F);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(3U, result->loot);
    EXPECT_EQ(WeaponType::ArcPulse, actor.weapon);
}
```

Add tests for no pickup outside radius, consumed loot, dead actor rejection, Rifle and ArcPulse replacement, MedKit healing 35, health cap 100, and NaN radius rejection.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: `Loot.hpp` is missing.

- [ ] Step 3: Implement one-item automatic pickup

Sort candidate pointers by `LootId`, choose the first active item within inclusive radius, mutate actor and item once, and return the before and after weapon or health values in `LootPickupResult`. Blade is not a world loot type because contenders start with it.

```cpp
struct LootItem
{
    LootId id = 0;
    LootType type = LootType::Rifle;
    Vec2 position;
    bool active = true;
};

struct LootPickupResult
{
    ActorId actor = 0;
    LootId loot = 0;
    LootType type = LootType::Rifle;
    std::optional<WeaponType> equippedWeapon;
    std::int32_t healedAmount = 0;
};
```

- [ ] Step 4: Run GREEN

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^Loot\.' --output-on-failure`

- [ ] Step 5: Commit

```powershell
git add simulation/CMakeLists.txt simulation/include/dxa/simulation/Loot.hpp simulation/src/Loot.cpp tests/CMakeLists.txt tests/simulation_loot_test.cpp
git commit -m "feat(loot): 무기 교체와 회복 pickup 추가" -m "이유: 이동 경로 선택이 전투 준비와 회복으로 이어지는 최소 파밍 규칙이 필요했습니다." -m "핵심 변경: LootId 우선 자동 pickup, 무기 교체, MedKit 회복과 소비 상태를 구현했습니다." -m "검증: Windows Debug build와 Loot focused test 통과 결과를 기록했습니다."
```

---

### Task 5: OfflineMatch lifecycle and deterministic spawn

Files:

- Create: `simulation/include/dxa/simulation/OfflineMatch.hpp`
- Create: `simulation/src/OfflineMatchInternal.hpp`
- Create: `simulation/src/OfflineMatch.cpp`
- Create: `simulation/src/OfflineMatchSpawn.cpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/simulation_offline_match_test.cpp`

Interfaces:

- Consumes: `NavMesh`, `MatchConfig`, `CombatActor`, `LootItem`.
- Produces: `OfflineMatch::Create`, `Start`, `Submit`, `Step`, `Snapshot`, `DrainEvents`.
- Produces for Task 6: private sorted actor vector, loot vector, queued command map, per-actor `NavAgent`.

- [ ] Step 1: Write failing start and spawn tests

```cpp
TEST(OfflineMatch, StartsTheCanonicalPopulationOnTheNavMesh)
{
    const NavMesh navMesh = MakeArenaNavMesh();
    OfflineMatch match = OfflineMatch::Create(navMesh, DefaultMatchConfig());
    match.Start();
    const MatchSnapshot snapshot = match.Snapshot();
    EXPECT_EQ(MatchPhase::Running, snapshot.phase);
    EXPECT_EQ(24U, CountRole(snapshot, ActorRole::Contender));
    EXPECT_EQ(100U, CountRole(snapshot, ActorRole::Neutral));
    EXPECT_EQ(50U, CountNeutral(snapshot, NeutralArchetype::Melee));
    EXPECT_EQ(50U, CountNeutral(snapshot, NeutralArchetype::Ranged));
    EXPECT_TRUE(AllMeleeNeutralsUse(snapshot, WeaponType::Blade, 60));
    EXPECT_TRUE(AllRangedNeutralsUse(snapshot, WeaponType::Rifle, 45));
    EXPECT_EQ(60U, snapshot.loot.size());
    EXPECT_TRUE(AllActorsOnNavMesh(snapshot, navMesh));
}

TEST(OfflineMatch, RepeatsSpawnAndLootForTheSameSeed)
{
    EXPECT_EQ(StartSnapshot(20260823U), StartSnapshot(20260823U));
    EXPECT_NE(StartSnapshot(20260823U), StartSnapshot(7U));
}
```

Add tests that actor zero is the controlled contender, actors and loot are ID-sorted, spawn spacing is respected, Start cannot run twice, Step before Start fails, a match created from a temporary NavMesh remains valid, and a too-small NavMesh fails within the bounded spawn-attempt count.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: `OfflineMatch.hpp` is missing.

- [ ] Step 3: Implement the deep match boundary

Public header:

```cpp
class OfflineMatch
{
public:
    [[nodiscard]] static OfflineMatch Create(
        const NavMesh& navMesh,
        MatchConfig config = DefaultMatchConfig());
    ~OfflineMatch();
    OfflineMatch(OfflineMatch&&) noexcept;
    OfflineMatch& operator=(OfflineMatch&&) noexcept;
    OfflineMatch(const OfflineMatch&) = delete;
    OfflineMatch& operator=(const OfflineMatch&) = delete;

    void Start();
    void Submit(MatchCommand command);
    void Step();
    [[nodiscard]] MatchSnapshot Snapshot() const;
    [[nodiscard]] std::vector<MatchEvent> DrainEvents();
private:
    struct Impl;
    explicit OfflineMatch(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};
```

Copy the supplied NavMesh into `Impl` before constructing any agent so `NavAgent` references point to match-owned storage and remain valid after the caller's mesh is destroyed. Reserve actor and agent containers before construction. Use a private 24-bit float conversion from `std::mt19937` and add the same GCC-safe result-type assertion used by the simulation benchmark. Spawn contenders around a ring, then neutrals and loot through bounded rejection sampling. Contenders start at 100 health with Blade, melee neutrals at 60 health with Blade, and ranged neutrals at 45 health with Rifle. Construct one `NavAgent` for every actor.

- [ ] Step 4: Run GREEN and Linux-sensitive build checks

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^OfflineMatch\.(Starts|Repeats|Rejects)' --output-on-failure`

Expected: spawn tests pass. No simulation source includes `Windows.h`, `d3d11.h`, or `DirectXMath.h`.

- [ ] Step 5: Commit

```powershell
git add simulation/CMakeLists.txt simulation/include/dxa/simulation/OfflineMatch.hpp simulation/src/OfflineMatchInternal.hpp simulation/src/OfflineMatch.cpp simulation/src/OfflineMatchSpawn.cpp tests/CMakeLists.txt tests/simulation_offline_match_test.cpp
git commit -m "feat(match): deterministic actor와 loot spawn 추가" -m "이유: 같은 seed의 경기 초기 상태를 재현하고 한 경기의 소유권을 한 경계에 모으기 위해 추가했습니다." -m "핵심 변경: match lifecycle, 소유 NavMesh, 24명과 중립 AI 100마리, loot spawn을 구현했습니다." -m "검증: Windows Debug build와 OfflineMatch spawn focused test 통과 결과를 기록했습니다."
```

---

### Task 6: Command validation, movement, and match-level pickup

Files:

- Modify: `simulation/src/OfflineMatchInternal.hpp`
- Modify: `simulation/src/OfflineMatch.cpp`
- Create: `simulation/src/OfflineMatchStep.cpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `tests/simulation_offline_match_test.cpp`

Interfaces:

- Consumes: queued `MatchCommand`, actor `NavAgent`, `ResolveNearestLootPickup`.
- Produces: one fixed tick of command selection, movement, pickup, cooldown decrement.
- Produces events: `CommandRejected`, `LootPickedUp`, `WeaponChanged`, `ActorHealed`.

- [ ] Step 1: Write failing command and movement tests

```cpp
TEST(OfflineMatch, UsesTheLastValidCommandPerActorInATick)
{
    OfflineMatch match = StartedSmallMatch();
    const Vec2 before = Actor(match.Snapshot(), 0U).position;
    const Vec2 first = before * -0.5F;
    const Vec2 last = before * -1.0F;
    match.Submit(MoveCommand(0U, first));
    match.Submit(MoveCommand(0U, last));
    match.Step();
    const Vec2 moved = Actor(match.Snapshot(), 0U).position - before;
    EXPECT_GT(Dot(moved, last - before), 0.0F);
}

TEST(OfflineMatch, RejectsOffMeshCommandWithoutStoppingTheMatch)
{
    OfflineMatch match = StartedSmallMatch();
    match.Submit(MoveCommand(0U, {100.0F, 100.0F}));
    match.Step();
    EXPECT_EQ(MatchPhase::Running, match.Snapshot().phase);
    EXPECT_TRUE(ContainsEvent(match.DrainEvents(), MatchEventType::CommandRejected));
}
```

Add dead actor, missing actor, missing attack target, non-finite destination, movement staying on NavMesh, exactly one fixed tick of displacement, automatic pickup, and stable event ordering tests.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: movement and command expectations fail because `Step()` is not connected.

- [ ] Step 3: Implement the first half of tick order

At the start of `Step()`, increment tick, select the final valid command for every actor, call `SetDestination`, tick every alive actor by `1.0F / tickRate`, copy positions back to `CombatActor`, resolve one loot item per contender, and decrement cooldowns. Clear the command queue even when every command is rejected.

Use `MatchEvent` fields rather than strings:

```cpp
struct MatchEvent
{
    std::uint32_t tick = 0;
    MatchEventType type = MatchEventType::CommandRejected;
    ActorId actor = 0;
    std::optional<ActorId> subject;
    std::optional<LootId> loot;
    std::int32_t amount = 0;
    std::optional<WeaponType> weapon;
};
```

Sort events by type, actor, optional subject, optional loot before hashing and exposure.

- [ ] Step 4: Run GREEN

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^OfflineMatch\.(Uses|Rejects|Moves|Picks)' --output-on-failure`

- [ ] Step 5: Commit

```powershell
git add simulation/CMakeLists.txt simulation/src/OfflineMatchInternal.hpp simulation/src/OfflineMatch.cpp simulation/src/OfflineMatchStep.cpp tests/simulation_offline_match_test.cpp
git commit -m "feat(match): command 이동과 자동 파밍 step 추가" -m "이유: 외부 입력을 고정 30Hz simulation에 안전하게 반영할 첫 tick 경로가 필요했습니다." -m "핵심 변경: command 검증, 마지막 명령 선택, NavAgent 이동, pickup과 cooldown 갱신을 연결했습니다." -m "검증: Windows Debug build와 OfflineMatch command focused test 통과 결과를 기록했습니다."
```

---

### Task 7: Match combat, zone damage, death, and result

Files:

- Create: `simulation/include/dxa/simulation/MatchResolution.hpp`
- Create: `simulation/src/MatchResolution.cpp`
- Modify: `simulation/src/OfflineMatchStep.cpp`
- Modify: `simulation/src/OfflineMatchInternal.hpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/simulation_match_resolution_test.cpp`
- Modify: `tests/simulation_offline_match_test.cpp`

Interfaces:

- Consumes: attack target commands, `ResolveAttacks`, `EvaluateSafeZone`, alive contenders.
- Produces events: `DamageApplied`, `ActorDied`, `MatchFinished`.
- Produces: `ActorId SelectSurvivalWinner(std::span<const ContenderRankInput>)`.
- Produces: finished `MatchResult` with winner and `LastSurvivor` or `TimeLimit`.

- [ ] Step 1: Write failing match-resolution tests

```cpp
TEST(OfflineMatch, AppliesAttacksBeforeZoneAndEmitsOneDeath)
{
    OfflineMatch match = StartedCombatFixture();
    match.Submit(AttackCommand(0U, 1U));
    StepUntilDeath(match, 1U);
    const auto events = match.DrainEvents();
    EXPECT_EQ(1U, CountDeathEvents(events, 1U));
    EXPECT_FALSE(Actor(match.Snapshot(), 1U).alive);
}

TEST(OfflineMatch, HardTimeoutLeavesOneRankedContender)
{
    OfflineMatch match = StartedTimeoutFixture();
    StepToTick(match, 18000U);
    const MatchSnapshot snapshot = match.Snapshot();
    ASSERT_TRUE(snapshot.result.has_value());
    EXPECT_EQ(MatchEndReason::TimeLimit, snapshot.result->reason);
    EXPECT_EQ(1U, snapshot.aliveContenders);
    EXPECT_EQ(snapshot.result->winner, SoleAliveContender(snapshot));
}
```

Add the pure ranking RED before the match integration assertions:

```cpp
TEST(MatchResolution, RanksAliveThenHealthThenEliminationsThenId)
{
    const std::array contenders{
        ContenderRankInput{9U, true, 80, 3U},
        ContenderRankInput{4U, true, 80, 3U},
        ContenderRankInput{1U, true, 100, 1U},
        ContenderRankInput{0U, false, 100, 99U}};
    EXPECT_EQ(1U, SelectSurvivalWinner(contenders));

    const std::array tied{
        ContenderRankInput{9U, true, 80, 3U},
        ContenderRankInput{4U, true, 80, 3U}};
    EXPECT_EQ(4U, SelectSurvivalWinner(tied));
}
```

Add pure ranking tests for an empty input and duplicate IDs. Add match tests for attacker cooldown, ArcPulse match events, neutral death not ending match, zone damage once per second, zone wipe preserving one ranked contender, immediate LastSurvivor finish, no Step after Finished, killer attribution, and event checksum changing for every event. `StartedCombatFixture` uses smaller configured contender spawn radii so every setup still enters through the public spawn path.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: combat and result tests fail because the second half of the tick is absent.

- [ ] Step 3: Complete the authoritative tick

Implement `SelectSurvivalWinner` as a total-order comparison over alive first, health descending, eliminations descending, and ActorId ascending. Reject empty input and duplicate IDs. Collect attack intents from validated commands after movement and pickup. Resolve attacks simultaneously. If the combat batch kills every remaining contender, rank the pre-combat states, restore one winner at health 1, remove that winner's death, and roll back only the elimination attributed to that death. Then apply whole-second zone damage to every alive actor. Convert damage and death records to match events. For a zone wipe, rank the pre-zone contender states; for timeout, rank the current states. Leave the selected winner at health 1.

Finish immediately when one contender remains. At tick 14,400, phase becomes `SuddenDeath`. At tick 18,000, force the timeout ranking if more than one remains.

- [ ] Step 4: Run GREEN and full simulation subset

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^(Combat|SafeZone|Loot|OfflineMatch)\.' --output-on-failure`

- [ ] Step 5: Commit

```powershell
git add simulation/CMakeLists.txt simulation/include/dxa/simulation/MatchResolution.hpp simulation/src/MatchResolution.cpp simulation/src/OfflineMatchInternal.hpp simulation/src/OfflineMatchStep.cpp tests/CMakeLists.txt tests/simulation_match_resolution_test.cpp tests/simulation_offline_match_test.cpp
git commit -m "feat(match): 전투 사망과 최후 생존자 판정 추가" -m "이유: 공격부터 사망과 결과까지 한 tick 안의 권위 순서를 완성해야 했습니다." -m "핵심 변경: combat batch, zone 피해, 처치 기록, 생존 순위와 최후 생존자 판정을 연결했습니다." -m "검증: Windows Debug build와 MatchResolution, Combat, SafeZone, Loot, OfflineMatch focused test 통과 결과를 기록했습니다."
```

---

### Task 8: Contender bots, neutral AI, and canonical full match

Files:

- Create: `simulation/include/dxa/simulation/OfflineBotController.hpp`
- Create: `simulation/src/OfflineMatchBots.cpp`
- Modify: `simulation/src/OfflineMatchInternal.hpp`
- Modify: `simulation/src/OfflineMatchStep.cpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/simulation_offline_match_bot_test.cpp`
- Modify: `tests/simulation_offline_match_test.cpp`

Interfaces:

- Consumes: actor and loot state, existing `BehaviorTreeAiController`, safe-zone state.
- Produces: `BotDecisionReason`, `BotPerception`, `BotDecision`, `DecideContender`, and `DecideNeutral`.
- Produces: one internal bot command for contender IDs 1 through 23 and every neutral actor every six ticks.
- Leaves contender ID 0 under external command ownership.
- Produces: deterministic canonical match result and event checksum.

- [ ] Step 1: Write failing decision-priority tests

Lock the testable public decision seam before writing the tests:

```cpp
enum class BotDecisionReason
{
    ReturnToZone,
    UseMedKit,
    SeekWeapon,
    Attack,
    Chase,
    Retreat,
    Idle
};

struct BotPerception
{
    std::span<const ActorSnapshot> actors;
    std::span<const LootSnapshot> loot;
    SafeZoneState safeZone;
};

struct BotDecision
{
    MatchCommand command;
    BotDecisionReason reason = BotDecisionReason::Idle;
};

[[nodiscard]] BotDecision DecideContender(
    const ActorSnapshot& self,
    const BotPerception& perception,
    const MatchConfig& config);

[[nodiscard]] BotDecision DecideNeutral(
    const ActorSnapshot& self,
    const BotPerception& perception,
    const MatchConfig& config,
    const BehaviorTreeAiController& controller);
```

```cpp
TEST(OfflineMatchBot, SafeZoneReturnOverridesLootAndCombat)
{
    const BotDecision decision = DecideContender(
        OutsideZoneLowHealthActor(),
        VisibleMedKitAndEnemy(),
        SafeZoneAt({0.0F, 0.0F}, 8.0F));
    ASSERT_TRUE(decision.command.moveDestination.has_value());
    EXPECT_EQ((Vec2{0.0F, 0.0F}), *decision.command.moveDestination);
}

TEST(OfflineMatchBot, ChoosesLowerIdOnEqualTargetDistance)
{
    const BotDecision decision = DecideContender(
        CenterActor(),
        EqualDistanceEnemies(7U, 3U),
        LargeSafeZone());
    EXPECT_EQ(3U, decision.command.attackTarget);
}
```

Add MedKit priority, weapon-upgrade priority, attack-in-range, chase-out-of-range, neutral only targeting contenders, ranged retreat, and no-target idle tests.

- [ ] Step 2: Run decision RED

Run: `./scripts/build.ps1`

Expected: `OfflineMatchBot` tests fail because bot decision functions are absent.

- [ ] Step 3: Implement 5Hz decisions

Build a `LooseQuadtree` from alive actors only on decision ticks. Contenders query radius 18 and neutrals query radius 10 from the locked config, then sort candidates by exact distance and ActorId. Build contender commands from the locked priority list. Build neutral blackboards and map the existing behavior-tree command to move, retreat, or attack targets.

Run internal contender decisions only for IDs 1 through 23. Never enqueue an internal command for actor 0. Store one `BehaviorTreeAiController` per neutral as `std::unique_ptr` because the existing controller is non-copyable and non-movable. `RunCanonicalMatch` calls the same public `DecideContender` function for actor 0 before each decision tick and submits that returned command through `OfflineMatch::Submit`; visible DX11 mode does not install this external auto-controller.

- [ ] Step 4: Write the canonical full-match RED

```cpp
TEST(OfflineMatch, CanonicalPopulationFinishesBetweenEightAndTenMinutes)
{
    const MatchSummary first = RunCanonicalMatch(20260823U);
    const MatchSummary repeated = RunCanonicalMatch(20260823U);
    EXPECT_EQ(first, repeated);
    EXPECT_GE(first.finishedTick, 14400U);
    EXPECT_LE(first.finishedTick, 18000U);
    EXPECT_EQ(1U, first.aliveContenders);
    EXPECT_TRUE(first.winner.has_value());
    EXPECT_NE(0U, first.eventChecksum);
    EXPECT_TRUE(first.allValuesFinite);
}
```

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^OfflineMatch\.Canonical' --output-on-failure`

Expected: RED until bot commands and balance produce a bounded result.

- [ ] Step 5: Diagnose a canonical-duration failure without weakening assertions

The first implementation used a 64×64 arena with contender speed 6, neutral speed 4.5, perception radii 18 and 10, contender spawn spacing 3, and neutral spacing 0.75. The canonical RED finished at tick 91; a diagnostic run without all neutral actors finished at tick 1,207. Event attribution showed 23 contender deaths, no zone deaths, 30 contender-sourced damage events, and 120 neutral-sourced damage events. A four-times uniform arena and zone scale finished at tick 10,561, while a five-times scale finished at tick 10,560. This confirmed that uniform scaling preserved the relative convergence time. After user approval, keep the 256×256 arena and use zone radii 128, 112, 96, 80, 64, and 0 so wide play lasts through tick 14,400. Leave timing, movement, perception, health, damage, cooldown, and the 14,400 to 18,000 acceptance unchanged.

- [ ] Step 6: Run GREEN and full suite

Run: `./scripts/build.ps1`

Run: `./scripts/test.ps1`

Expected: canonical result repeats and the full suite passes.

- [ ] Step 7: Commit

```powershell
git add simulation/CMakeLists.txt simulation/include/dxa/simulation/OfflineBotController.hpp simulation/src/OfflineMatchInternal.hpp simulation/src/OfflineMatchStep.cpp simulation/src/OfflineMatchBots.cpp tests/CMakeLists.txt tests/simulation_offline_match_bot_test.cpp tests/simulation_offline_match_test.cpp
git commit -m "feat(ai): 24인 offline match bot 완주 추가" -m "이유: 사용자 actor와 같은 command 경계를 사용하는 자동 참가자로 전체 경기 재현이 필요했습니다." -m "핵심 변경: 경쟁 봇 우선순위, 중립 behavior tree 연동과 canonical auto-controller를 구현했습니다." -m "검증: canonical 반복 결과와 전체 Windows Debug test 통과 결과를 기록했습니다."
```

---

### Task 9: Generic renderer actor state and live zone seam

Files:

- Modify: `engine/include/dxa/engine/HybridDeferredRenderer.hpp`
- Modify: `engine/src/windows/HybridDeferredRenderer.cpp`
- Modify: `engine/include/dxa/engine/Window.hpp`
- Modify: `engine/src/windows/Window.cpp`
- Modify: `tests/engine_hybrid_deferred_renderer_test.cpp`
- Modify: `tests/engine_window_test.cpp`

Interfaces:

- Consumes: generic engine positions and active flags, not match types.
- Produces: `SetPlayerStates`, `SetAiStates`, `SetZoneRadius`, `Window::SetTitle`.

- [ ] Step 1: Write failing renderer and title tests

```cpp
TEST(HybridDeferredRenderer, AppliesGenericCharacterStatesWithoutChangingStressDefaults)
{
    HybridDeferredRenderer renderer = InitializedWarpRenderer();
    const RenderStatistics baseline = RenderOneFrame(renderer);
    std::vector<SceneCharacterState> players(PlayerCount);
    players[0] = {{20.0F, 0.0F, 10.0F}, true};
    players[1] = {{0.0F, 0.0F, 0.0F}, false};
    renderer.SetPlayerStates(players);
    renderer.SetZoneRadius(7.0F);
    const RenderStatistics updated = RenderOneFrame(renderer);
    EXPECT_LT(updated.objectCount, baseline.objectCount);
    EXPECT_TRUE(RenderOneMoreFrameWithoutDebugErrors(renderer));
}

TEST(Window, UpdatesTheVisibleMatchStatusTitle)
{
    Window window = HiddenTestWindow();
    window.SetTitle(L"Alive 7 | Rifle");
    EXPECT_EQ(L"Alive 7 | Rifle", NativeWindowTitle(window.NativeHandle()));
}
```

Add size mismatch, non-finite position, negative radius, and calls before initialization tests.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: new engine methods are missing.

- [ ] Step 3: Implement generic state updates

```cpp
struct SceneCharacterState
{
    benchmark::SceneVector3 position;
    bool active = true;
};

void SetPlayerStates(std::span<const SceneCharacterState> states);
void SetAiStates(std::span<const SceneCharacterState> states);
void SetZoneRadius(float radius);
```

Require state counts to equal existing stress-scene slot counts. Initialize private `playerActive_` and `aiActive_` arrays to `true`. Both shadow and G-Buffer character loops pair each scene instance with its active flag and skip inactive slots before culling or statistics accumulation. Do not create zero-scale world matrices. Existing stress-scene behavior remains unchanged until a state setter is called, while an offline-match death removes that slot from rendering and object counts. The default zone animation remains active until `SetZoneRadius` supplies an override.

`Window::SetTitle` rejects an empty title and calls `SetWindowTextW`; failure throws.

- [ ] Step 4: Run GREEN

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^(HybridDeferredRenderer|Window)\.' --output-on-failure`

- [ ] Step 5: Commit

```powershell
git add engine/include/dxa/engine/HybridDeferredRenderer.hpp engine/src/windows/HybridDeferredRenderer.cpp engine/include/dxa/engine/Window.hpp engine/src/windows/Window.cpp tests/engine_hybrid_deferred_renderer_test.cpp tests/engine_window_test.cpp
git commit -m "feat(client): match actor와 zone renderer seam 추가" -m "이유: simulation type을 renderer에 노출하지 않고 경기 snapshot을 표시할 입력 경계가 필요했습니다." -m "핵심 변경: 일반 character state, inactive slot, zone radius와 window title 갱신을 추가했습니다." -m "검증: WARP renderer와 Window focused test 통과 결과를 기록했습니다."
```

---

### Task 10: DX11 offline match demo and WARP completion smoke

Files:

- Create: `cmake/DeployHybridRuntime.cmake`
- Create: `apps/offline_match_demo/CMakeLists.txt`
- Create: `apps/offline_match_demo/src/main.cpp`
- Modify: `CMakeLists.txt`
- Modify: `apps/navigation_demo/CMakeLists.txt`

Interfaces:

- Consumes: `OfflineMatch`, pointer ground picking, renderer state seams.
- Produces executable: `dxa_offline_match_demo`.
- Produces CTest: `OfflineMatchDemo.WarpSmoke`.

- [ ] Step 1: Add a failing CTest registration

```cmake
add_test(
    NAME OfflineMatchDemo.WarpSmoke
    COMMAND
        dxa_offline_match_demo
        --warp
        --hidden
        --auto-match
        --verify-match
)
set_tests_properties(OfflineMatchDemo.WarpSmoke PROPERTIES TIMEOUT 120)
```

Run: `./scripts/build.ps1`

Expected: configure fails because `apps/offline_match_demo` and the target do not exist.

- [ ] Step 2: Implement CLI and two execution modes

CLI:

```text
dxa_offline_match_demo [--warp] [--hidden] [--auto-match]
                       [--verify-match] [--seed N]
```

Rules:

- `--hidden` requires `--auto-match`.
- `--verify-match` requires `--auto-match`.
- visible mode uses a real-time accumulator and calls `Step()` zero or more times until caught up, with a maximum of five catch-up ticks per rendered frame.
- right click submits player movement.
- app selects the nearest visible hostile from the latest snapshot for player attack commands.
- auto mode runs all simulation ticks without rendering between them, then renders start and result frames.
- auto mode submits `DecideContender` output for actor 0 before every six-tick decision boundary; visible mode never does this.
- renderer arrays always contain 24 player slots and 100 neutral slots.
- final console line prints tick, seconds, winner, reason, checksum.

- [ ] Step 3: Deploy shaders and runtime assets

Create `cmake/DeployHybridRuntime.cmake` with one function:

```cmake
function(dxa_deploy_hybrid_runtime target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "unknown hybrid runtime target: ${target}")
    endif()
    set(
        shaders
        hybrid_geometry.hlsl
        hybrid_lighting.hlsl
        hybrid_shadow.hlsl
        hybrid_transparent.hlsl
    )
    foreach(shader IN LISTS shaders)
        add_custom_command(
            TARGET ${target}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target}>/shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/assets/shaders/${shader}"
                "$<TARGET_FILE_DIR:${target}>/shaders/${shader}"
            VERBATIM
        )
    endforeach()
    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/assets/runtime"
            "$<TARGET_FILE_DIR:${target}>/assets"
        VERBATIM
    )
endfunction()
```

Move the existing navigation-demo copy commands into this function without changing their source or destination layout. Include the helper and call `dxa_deploy_hybrid_runtime(dxa_navigation_demo)` from the navigation demo, then call `dxa_deploy_hybrid_runtime(dxa_offline_match_demo)` from the new demo. Link `dxa_engine`, `dxa_navigation_demo_core`, and `dxa_simulation`. Keep `NavigationDemo.WarpSmoke` in the focused test run so the extraction cannot silently break week 6 deployment.

- [ ] Step 4: Run WARP GREEN and full suite

Run: `./scripts/build.ps1`

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^(NavigationDemo|OfflineMatchDemo)\.WarpSmoke$' --output-on-failure`

Run: `./scripts/test.ps1`

Expected: WARP smoke prints a bounded result, reads a non-clear pixel, reports no DX11 debug error, and the full suite passes.

- [ ] Step 5: Commit

```powershell
git add CMakeLists.txt cmake/DeployHybridRuntime.cmake apps/navigation_demo/CMakeLists.txt apps/offline_match_demo/CMakeLists.txt apps/offline_match_demo/src/main.cpp
git commit -m "feat(client): DX11 offline match 완주 demo 추가" -m "이유: 실제 입력 화면과 자동 WARP 검증이 같은 경기 loop를 실행하는 수직 기능이 필요했습니다." -m "핵심 변경: visible 및 auto 실행 모드, snapshot adapter, 공유 runtime 배포와 결과 검증을 추가했습니다." -m "검증: NavigationDemo와 OfflineMatchDemo WARP smoke 및 전체 test 통과 결과를 기록했습니다."
```

---

### Task 11: Canonical match Release benchmark and immutable evidence

Files:

- Create: `apps/offline_match_benchmark/CMakeLists.txt`
- Create: `apps/offline_match_benchmark/include/dxa/offline_match_benchmark/BenchmarkOptions.hpp`
- Create: `apps/offline_match_benchmark/src/main.cpp`
- Create: `scripts/offline_match_benchmark_common.ps1`
- Create: `scripts/run_offline_match_benchmark.ps1`
- Create: `tests/offline_match_benchmark_options_test.cpp`
- Create: `tests/offline_match_benchmark_runner_test.ps1`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: canonical `OfflineMatch`.
- Produces executable: `dxa_offline_match_benchmark`.
- Produces raw files: `ticks.csv`, `result.json`, `environment.json`.

- [ ] Step 1: Write failing option and runner guard tests

```cpp
TEST(OfflineMatchBenchmarkOptions, ParsesRequiredEvidenceBoundary)
{
    const auto result = ParseOfflineMatchBenchmarkOptions({
        "--output", "run", "--commit-sha", "abc", "--seed", "20260823"});
    ASSERT_TRUE(result.options.has_value());
    EXPECT_EQ(20260823U, result.options->seed);
    EXPECT_EQ("abc", result.options->commitSha);
}
```

PowerShell tests reject dirty trees, moved HEAD, existing output, wrong commit SHA, nonzero mismatch, missing winner, finish tick outside 14,400 to 18,000, missing checksum, wrong CSV row count, non-finite tick sample, and absent CPU or compiler fields.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: options header and runner common script are missing.

- [ ] Step 3: Implement platform-neutral timing

Run the canonical match once for validation and once for measurement. Validation compares two same-seed summaries before timing. Measurement records one row per tick:

```csv
tick,elapsed_ms,alive_contenders,alive_neutrals,event_count
```

Use nearest-rank P50 and P95 over all tick samples. JSON schema:

```json
{
  "schema_version": 1,
  "commit_sha": "...",
  "seed": 20260823,
  "winner": 0,
  "end_reason": "last_survivor",
  "finished_tick": 0,
  "event_checksum": "...",
  "repeat_mismatch_count": 0,
  "tick_ms": {"p50": 0.0, "p95": 0.0, "max": 0.0},
  "population": {"contenders": 24, "neutrals": 100}
}
```

If summaries differ, return exit code 3 before creating output. Refuse existing output paths.

- [ ] Step 4: Run Debug, full suite, and Release build

Run: `./scripts/build.ps1`

Run: `./scripts/test.ps1`

Run: `./scripts/build.ps1 -Preset windows-msvc-release`

Expected: all commands pass. Local GCC runs only when an installed compiler or Docker engine is available; otherwise defer to Ubuntu CI and record that fact.

- [ ] Step 5: Commit benchmark code before measurement

```powershell
git add CMakeLists.txt apps/offline_match_benchmark/CMakeLists.txt apps/offline_match_benchmark/include/dxa/offline_match_benchmark/BenchmarkOptions.hpp apps/offline_match_benchmark/src/main.cpp scripts/offline_match_benchmark_common.ps1 scripts/run_offline_match_benchmark.ps1 tests/CMakeLists.txt tests/offline_match_benchmark_options_test.cpp tests/offline_match_benchmark_runner_test.ps1
git commit -m "feat(benchmark): offline match tick runner 추가" -m "이유: 경기 완주 시간과 simulation tick 비용을 같은 seed와 commit에서 반복 측정해야 했습니다." -m "핵심 변경: Release tick 측정, repeat 비교, immutable output과 runner guard를 구현했습니다." -m "검증: Debug 전체 test와 MSVC Release build 통과 결과를 기록했습니다."
```

- [ ] Step 6: Run official Release benchmark

Run: `./scripts/run_offline_match_benchmark.ps1`

Expected: clean commit accepted, repeat mismatch 0, winner present, finish tick in range, P95 at or below 33.3ms, new timestamped directory, validation `passed`.

---

### Task 12: ADR, devlog, review, and week 7 PR

Files:

- Create: `docs/adr/0005-authoritative-offline-match.md`
- Create: `docs/devlog/2026-08-24-offline-match-loop.md`
- Create: `docs/benchmarks/offline-match/{yyyyMMdd-HHmmss}-{short-sha}-seed{seed}/RESULT.md`
- Modify: `docs/benchmarks/README.md`
- Modify: `README.md`
- Modify: `docs/PROJECT_PLAN.md`
- Include raw: `docs/benchmarks/offline-match/{yyyyMMdd-HHmmss}-{short-sha}-seed{seed}/ticks.csv`
- Include raw: `docs/benchmarks/offline-match/{yyyyMMdd-HHmmss}-{short-sha}-seed{seed}/result.json`
- Include raw: `docs/benchmarks/offline-match/{yyyyMMdd-HHmmss}-{short-sha}-seed{seed}/environment.json`

Interfaces:

- Consumes: actual commits, tests, WARP output, benchmark raw.
- Produces: week 7 evidence and PR description.

- [ ] Step 1: Write records from actual evidence

ADR records why `OfflineMatch` owns the tick order, why damage is batched, why no ECS was added, and why timeout ranking exists. Devlog order is situation, baseline, first playable loop, encountered failures, alternatives, implementation, result, limitations.

Include the exact canonical winner, finished tick and seconds, reason, event checksum, tick P50/P95/max, compiler, CPU, commit SHA, WARP result, and full test count. Do not claim the behavior tree is faster or that the timeout path is a true combat victory.

- [ ] Step 2: Update README and project status

README adds visible and WARP demo commands and states combat and offline match are implemented while networking is not. Project plan marks week 7 complete and week 8 lobby and rooms as next only after raw validation and docs checks pass.

- [ ] Step 3: Validate documentation and raw

Parse JSON, import CSV, require expected row count equal to `finished_tick`, validate checksum and SHA, verify all local links, scan for unfinished markers, run `git diff --check`, and ensure prior benchmark directories are unchanged.

- [ ] Step 4: Commit evidence

```powershell
git add README.md docs/PROJECT_PLAN.md docs/adr/0005-authoritative-offline-match.md docs/devlog/2026-08-24-offline-match-loop.md docs/benchmarks/README.md docs/benchmarks/offline-match
git commit -m "docs(match): 오프라인 경기 완주 원본 기록" -m "이유: 코드 설명과 포트폴리오 수치가 재현 가능한 원본에 연결되어야 했습니다." -m "핵심 변경: ADR, 개발 기록, README, project status와 새 benchmark raw를 실제 결과로 정리했습니다." -m "검증: JSON, CSV, SHA, link, 이전 raw 불변성과 전체 test 재검증 결과를 기록했습니다."
```

- [ ] Step 5: Review from week 6 merge-base

Review from `4d4ff671b1654820c76e9d8eedc4db7ba38ee04d` with correctness, testing, maintainability, performance, reliability, and adversarial evidence lenses. Apply only reproduced findings with focused RED and GREEN tests. When a finding exists, use a concrete noun-form subject such as `fix(review): 경기 invariant 검증 보강`; create no review commit when no finding survives validation.

- [ ] Step 6: Fresh verification

Run: `./scripts/build.ps1`

Run: `./scripts/test.ps1`

Run: `./scripts/build.ps1 -Preset windows-msvc-release`

Run the official raw validators again without creating or overwriting a run.

- [ ] Step 7: Push and open the PR

Push `feat/offline-match-loop`, open a merge-commit PR to `main`, and monitor Windows and Ubuntu CI until merge-ready. Resolve current-head CI failures and review feedback with isolated commits. Do not merge without a new user instruction.

---

## Expected Commit Sequence

1. `33da430 docs(simulation): 7주차 오프라인 경기 설계 확정`
2. `docs(simulation): 7주차 구현 계획 고정`
3. `feat(match): 오프라인 경기 domain과 config 추가`
4. `feat(combat): 무기와 동시 피해 판정 추가`
5. `feat(zone): 4단계 축소 구역과 sudden death 추가`
6. `feat(loot): 무기 교체와 회복 pickup 추가`
7. `feat(match): deterministic actor와 loot spawn 추가`
8. `feat(match): command 이동과 자동 파밍 step 추가`
9. `feat(match): 전투 사망과 최후 생존자 판정 추가`
10. `feat(ai): 24인 offline match bot 완주 추가`
11. `feat(client): match actor와 zone renderer seam 추가`
12. `feat(client): DX11 offline match 완주 demo 추가`
13. `feat(benchmark): offline match tick runner 추가`
14. `docs(match): 오프라인 경기 완주 원본 기록`
15. Review fixes only when reproduced

Every implementation commit body includes the observed RED failure, focused GREEN count, full-suite count when run, and any deferred Linux validation. The sequence is evidence-driven; do not create empty commits to match this list.
