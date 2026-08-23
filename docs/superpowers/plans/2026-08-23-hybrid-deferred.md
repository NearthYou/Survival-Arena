# Hybrid Deferred Rendering Implementation Plan

> For agentic workers: REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

Goal: 포워드 기준선을 보존한 채 그림자와 하이브리드 디퍼드 렌더링을 추가하고 같은 RTX 장면에서 pass별 결과를 비교한다.

Architecture: `AssetSceneRenderer`는 forward baseline으로 유지하고 별도 `HybridDeferredRenderer`를 선택한다. 플랫폼 중립 frustum과 render path 계약은 benchmark library에 두고, Windows renderer는 shadow, G-Buffer, lighting, transparent pass와 static instance buffer를 소유한다.

Tech Stack: C++20, DirectX 11, HLSL 5.0, CMake, GoogleTest, PowerShell, DX11 timestamp query

Spec: `docs/superpowers/specs/2026-08-23-hybrid-deferred-design.md`

## Global Constraints

- forward 기본 동작과 4주차 raw benchmark 파일을 수정하지 않는다.
- scene seed는 `20260823`, camera 시간은 60Hz frame index에서 계산한다.
- hybrid G-Buffer는 albedo와 roughness, oct normal, sample 가능한 depth를 저장한다.
- static 1,000개만 GPU instancing한다. character palette instancing은 범위 밖이다.
- benchmark 실행은 기존 디렉터리를 덮어쓰지 않는다.
- Windows renderer 코드는 Linux target에 포함하지 않는다.
- 모든 production behavior는 먼저 실패하는 테스트를 확인한다.

---

### Task 1: Render path와 schema 2 계약

Files:
- Create: `engine/include/dxa/engine/RenderPath.hpp`
- Modify: `apps/client/include/dxa/client/ClientOptions.hpp`
- Modify: `apps/client/CMakeLists.txt`
- Modify: `engine/include/dxa/engine/benchmark/BenchmarkReport.hpp`
- Modify: `engine/src/benchmark/BenchmarkReport.cpp`
- Modify: `tests/client_options_test.cpp`
- Modify: `tests/engine_benchmark_report_test.cpp`

Interfaces:
- Produces: `enum class RenderPath { Forward, HybridDeferred }`
- Produces: `std::string_view ToString(RenderPath) noexcept`
- Produces: `ClientOptions::renderPath`
- Produces: schema 2 `FrameSample` pass metrics and pass draw counts

- [ ] Step 1: Write failing render path option tests

```cpp
TEST(ClientOptions, ParsesHybridDeferredRenderPath)
{
    constexpr std::array arguments{
        std::string_view{"--render-path"},
        std::string_view{"hybrid-deferred"}};
    const auto result = ParseClientOptions(arguments);
    ASSERT_TRUE(result.options.has_value()) << result.error;
    EXPECT_EQ(dxa::engine::RenderPath::HybridDeferred, result.options->renderPath);
}

TEST(ClientOptions, RejectsUnknownRenderPath)
{
    constexpr std::array arguments{
        std::string_view{"--render-path"},
        std::string_view{"path-tracing"}};
    const auto result = ParseClientOptions(arguments);
    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ("--render-path must be forward or hybrid-deferred", result.error);
}
```

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: compile failure because `RenderPath` and `renderPath` do not exist.

- [ ] Step 3: Add the render path header and parser

```cpp
namespace dxa::engine
{
enum class RenderPath
{
    Forward,
    HybridDeferred
};

[[nodiscard]] constexpr std::string_view ToString(const RenderPath path) noexcept
{
    return path == RenderPath::Forward ? "forward" : "hybrid-deferred";
}
}
```

`dxa_client_options` links `dxa_engine_core` as an interface dependency so the header resolves on Windows and Linux.

- [ ] Step 4: Write failing schema 2 report test

```cpp
const FrameSample sample{
    .frameIndex = 121,
    .cpuFrameMilliseconds = 4.0,
    .gpuTotalMilliseconds = 3.0,
    .gpuShadowMilliseconds = 0.5,
    .gpuGBufferMilliseconds = 1.0,
    .gpuLightingMilliseconds = 1.25,
    .gpuTransparentMilliseconds = 0.25,
    .drawCalls = 1400,
    .shadowDrawCalls = 125,
    .gBufferDrawCalls = 1273,
    .lightingDrawCalls = 1,
    .transparentDrawCalls = 1,
    .triangleCount = 1000,
    .objectCount = 1124,
    .visibleObjectCount = 700,
    .culledObjectCount = 424,
    .workingSetBytes = 2000};
```

Assert CSV contains `gpu_total_ms,gpu_shadow_ms,gpu_gbuffer_ms,gpu_lighting_ms,gpu_transparent_ms` and JSON contains `"schema_version": 2` plus `"render_path": "hybrid-deferred"`.

- [ ] Step 5: Run schema RED

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R "^(ClientOptions|BenchmarkReport)\\." --output-on-failure`

Expected: compile failure on the new fields.

- [ ] Step 6: Implement schema 2 while keeping existing column names

Add optional GPU fields and pass counters to `FrameSample`. Add pass `MetricSummary` members to `FrameSummary`. Add `RenderPath renderPath` to `BenchmarkMetadata`. Write empty CSV cells for unavailable pass values and zero summaries when a path does not use a pass.

- [ ] Step 7: Run GREEN and full regression

Run: `./scripts/build.ps1`

Run: `./scripts/test.ps1`

Expected: all tests pass and the forward WARP smoke stays green.

- [ ] Step 8: Commit

```powershell
git add apps/client/include/dxa/client/ClientOptions.hpp apps/client/CMakeLists.txt engine/include/dxa/engine/RenderPath.hpp engine/include/dxa/engine/benchmark/BenchmarkReport.hpp engine/src/benchmark/BenchmarkReport.cpp tests/client_options_test.cpp tests/engine_benchmark_report_test.cpp
git commit -m "feat(benchmark): 렌더 경로와 pass 측정 계약 추가"
```

---

### Task 2: Perspective frustum과 deterministic visible set

Files:
- Create: `engine/include/dxa/engine/benchmark/PerspectiveFrustum.hpp`
- Create: `engine/src/benchmark/PerspectiveFrustum.cpp`
- Create: `tests/engine_perspective_frustum_test.cpp`
- Modify: `engine/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:
- Consumes: `benchmark::SceneVector3`, `benchmark::StressCamera`
- Produces: `BoundingSphere`, `PerspectiveFrustum`, `BuildPerspectiveFrustum`, `IntersectsSphere`

- [ ] Step 1: Write failing frustum tests

```cpp
const StressCamera camera{
    SceneVector3{0.0F, 0.0F, 0.0F},
    SceneVector3{0.0F, 0.0F, 1.0F}};
const auto frustum = BuildPerspectiveFrustum(
    camera, std::numbers::pi_v<float> / 2.0F, 1.0F, 1.0F, 10.0F);

EXPECT_TRUE(frustum.IntersectsSphere({SceneVector3{0.0F, 0.0F, 5.0F}, 0.5F}));
EXPECT_FALSE(frustum.IntersectsSphere({SceneVector3{0.0F, 0.0F, 11.0F}, 0.5F}));
EXPECT_TRUE(frustum.IntersectsSphere({SceneVector3{5.4F, 0.0F, 5.0F}, 0.5F}));
EXPECT_FALSE(frustum.IntersectsSphere({SceneVector3{6.0F, 0.0F, 5.0F}, 0.5F}));
```

Add tests for invalid FOV, zero aspect, near not below far, and the same camera producing the same 1,124-entry visibility mask.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: missing `PerspectiveFrustum.hpp`.

- [ ] Step 3: Implement normalized camera basis and six plane sphere tests

```cpp
struct Plane
{
    SceneVector3 normal;
    float distance = 0.0F;
};

class PerspectiveFrustum
{
public:
    [[nodiscard]] bool IntersectsSphere(const BoundingSphere& sphere) const noexcept;
private:
    std::array<Plane, 6> planes_;
};
```

Reject a sphere only when `dot(plane.normal, sphere.center) + plane.distance < -sphere.radius` for one plane.

- [ ] Step 4: Run GREEN and GCC warning build

Run: `./scripts/build.ps1`

Run: `docker run --rm --mount "type=bind,source=$PWD,target=/src,readonly" gcc:13 g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror -I/src/engine/include -c /src/engine/src/benchmark/PerspectiveFrustum.cpp -o /tmp/PerspectiveFrustum.o`

Expected: frustum tests and GCC compile pass.

- [ ] Step 5: Commit

```powershell
git add engine/CMakeLists.txt engine/include/dxa/engine/benchmark/PerspectiveFrustum.hpp engine/src/benchmark/PerspectiveFrustum.cpp tests/CMakeLists.txt tests/engine_perspective_frustum_test.cpp
git commit -m "feat(culling): perspective frustum 교차 검사 추가"
```

---

### Task 3: Pass별 비동기 GPU timestamp

Files:
- Create: `engine/include/dxa/engine/RenderPass.hpp`
- Modify: `engine/include/dxa/engine/GpuFrameTimer.hpp`
- Modify: `engine/src/windows/GpuFrameTimer.cpp`
- Modify: `tests/engine_gpu_frame_timer_test.cpp`

Interfaces:
- Produces: `RenderPass::{Forward, Shadow, GBuffer, DeferredLighting, Transparent}`
- Produces: `GpuFrameTimer::MarkPass(ID3D11DeviceContext*, RenderPass)`
- Produces: `GpuFrameResult` total and pass duration optionals

- [ ] Step 1: Write failing marker order and duration tests

Create a platform-neutral helper in the header:

```cpp
struct TimestampSequence
{
    std::uint64_t frequency = 0;
    std::uint64_t start = 0;
    std::array<std::uint64_t, 4> markers{};
    std::uint64_t end = 0;
    std::size_t markerCount = 0;
};

[[nodiscard]] GpuPassDurations CalculatePassDurations(
    const TimestampSequence& sequence,
    std::span<const RenderPass> passes);
```

Assert timestamps `100, 120, 150, 190, 200` at frequency `1000` produce 20ms shadow, 30ms G-Buffer, 40ms lighting, 10ms transparent and 100ms total. Assert duplicate or out-of-order pass markers throw.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: missing pass timing types.

- [ ] Step 3: Implement pure duration validation

Require increasing timestamps, exact hybrid pass order, non-zero frequency and at most four markers. Forward accepts zero markers and maps total to forward.

- [ ] Step 4: Extend query slots

Each slot creates start, end and four marker timestamp queries. `BeginFrame` resets marker count. `MarkPass` records the next marker and pass. `TryResolve` reads only the recorded markers after disjoint resolves.

- [ ] Step 5: Run WARP GREEN

Update the WARP timer test to mark four passes between begin and end and assert all returned pass values exist and are non-negative.

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^GpuFrameTimer\\.' --output-on-failure`

Expected: all GPU timer tests pass.

- [ ] Step 6: Commit

```powershell
git add engine/include/dxa/engine/RenderPass.hpp engine/include/dxa/engine/GpuFrameTimer.hpp engine/src/windows/GpuFrameTimer.cpp tests/engine_gpu_frame_timer_test.cpp
git commit -m "feat(benchmark): GPU pass timestamp 분리"
```

---

### Task 4: G-Buffer와 deferred lighting vertical slice

Files:
- Create: `engine/include/dxa/engine/HybridDeferredRenderer.hpp`
- Create: `engine/src/windows/HybridDeferredRenderer.cpp`
- Create: `assets/shaders/hybrid_geometry.hlsl`
- Create: `assets/shaders/hybrid_lighting.hlsl`
- Create: `tests/engine_hybrid_deferred_renderer_test.cpp`
- Modify: `engine/include/dxa/engine/AssetSceneRenderer.hpp`
- Modify: `engine/include/dxa/engine/GraphicsDevice.hpp`
- Modify: `engine/src/windows/GraphicsDevice.cpp`
- Modify: `engine/CMakeLists.txt`
- Modify: `apps/client/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:
- Consumes: `AssetSceneFrame`, `RenderStatistics`, `StressScene`, runtime `.dxam`
- Produces: `HybridDeferredRenderer::Initialize`, `HybridDeferredRenderer::Render`
- Produces: `GraphicsDevice::BackBufferRenderTargetView()`

- [ ] Step 1: Write failing WARP G-Buffer smoke

```cpp
renderer.Initialize(
    graphics.Device(),
    HybridDeferredConfig{
        96, 54, 128, 20260823U,
        std::filesystem::path{DXA_TEST_SHADER_ROOT},
        std::filesystem::path{DXA_TEST_ASSET_ROOT}});

graphics.BeginFrame(ClearColor);
const RenderStatistics stats = renderer.Render(
    graphics.Context(),
    graphics.BackBufferRenderTargetView(),
    AssetSceneFrame{1, 0.0, 96.0F / 54.0F});
EXPECT_GT(stats.gBufferDrawCalls, 0U);
EXPECT_EQ(1U, stats.lightingDrawCalls);
EXPECT_TRUE(graphics.BackBufferContainsNonClearPixel(ClearColor));
```

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: missing `HybridDeferredRenderer` and back buffer accessor.

- [ ] Step 3: Add non-owning back buffer accessor

```cpp
ID3D11RenderTargetView* GraphicsDevice::BackBufferRenderTargetView() const noexcept
{
    return renderTargetView_.Get();
}
```

- [ ] Step 4: Create G-Buffer resources

Create two RTV and SRV textures at config width and height. Create typeless depth with DSV and SRV. All sample counts are one. Create point sampler for G-Buffer and linear sampler for albedo.

- [ ] Step 5: Implement geometry pass

`hybrid_geometry.hlsl` writes:

```hlsl
struct GBufferOutput
{
    float4 albedoRoughness : SV_TARGET0;
    float2 octNormal : SV_TARGET1;
};
```

Character skinning uses the same 64-matrix palette contract as the forward shader.

- [ ] Step 6: Implement fullscreen lighting

Use `SV_VertexID` fullscreen triangle. Reconstruct world position from depth and `InverseViewProjection`. Apply directional and point lights. Write to the supplied back buffer RTV.

- [ ] Step 7: Run GREEN

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^HybridDeferredRenderer\\.' --output-on-failure`

Expected: WARP image contains non-clear pixels and G-Buffer plus lighting draw counts are non-zero.

- [ ] Step 8: Commit

```powershell
git add engine/include/dxa/engine/GraphicsDevice.hpp engine/src/windows/GraphicsDevice.cpp engine/include/dxa/engine/HybridDeferredRenderer.hpp engine/src/windows/HybridDeferredRenderer.cpp engine/CMakeLists.txt assets/shaders/hybrid_geometry.hlsl assets/shaders/hybrid_lighting.hlsl apps/client/CMakeLists.txt tests/CMakeLists.txt tests/engine_hybrid_deferred_renderer_test.cpp
git commit -m "feat(renderer): G-Buffer와 deferred lighting 연결"
```

---

### Task 5: Directional shadow pass

Files:
- Create: `assets/shaders/hybrid_shadow.hlsl`
- Modify: `engine/include/dxa/engine/HybridDeferredRenderer.hpp`
- Modify: `engine/src/windows/HybridDeferredRenderer.cpp`
- Modify: `assets/shaders/hybrid_lighting.hlsl`
- Modify: `apps/client/CMakeLists.txt`
- Modify: `tests/engine_hybrid_deferred_renderer_test.cpp`

Interfaces:
- Produces: shadow DSV and SRV, light view projection, comparison sampler
- Produces: `RenderStatistics::shadowDrawCalls`

- [ ] Step 1: Extend WARP test with shadow assertions

Assert `shadowDrawCalls > 0`, shadow resource readiness is true, and the shadow shader deployment test is registered.

- [ ] Step 2: Run RED

Run: `./scripts/build.ps1`

Expected: missing shadow methods or zero shadow draw count.

- [ ] Step 3: Create shadow resource and state

Use `R32_TYPELESS` texture, `D32_FLOAT` DSV and `R32_FLOAT` SRV. Create a comparison sampler with border depth 1 and a rasterizer state with positive depth bias.

- [ ] Step 4: Render shadow depth

Bind no color target and the shadow DSV. Clear depth. In this vertical slice draw each static object and character once using the whole model index range. Restore the camera viewport after the pass. Task 6 replaces the 1,000 static shadow draws with one instanced draw.

- [ ] Step 5: Sample shadow in lighting

Transform reconstructed world position by light view projection and use 3×3 PCF comparison samples. Outside shadow UV returns fully lit.

- [ ] Step 6: Run GREEN and debug layer smoke

Run: `./scripts/test.ps1`

Expected: shadow WARP test and all existing tests pass without DX11 debug errors.

- [ ] Step 7: Commit

```powershell
git add assets/shaders/hybrid_shadow.hlsl assets/shaders/hybrid_lighting.hlsl engine/include/dxa/engine/HybridDeferredRenderer.hpp engine/src/windows/HybridDeferredRenderer.cpp apps/client/CMakeLists.txt tests/engine_hybrid_deferred_renderer_test.cpp
git commit -m "feat(renderer): 방향광 shadow pass 추가"
```

---

### Task 6: Static instancing, frustum culling, transparent pass

Files:
- Create: `assets/shaders/hybrid_transparent.hlsl`
- Modify: `assets/shaders/hybrid_geometry.hlsl`
- Modify: `assets/shaders/hybrid_shadow.hlsl`
- Modify: `engine/include/dxa/engine/HybridDeferredRenderer.hpp`
- Modify: `engine/src/windows/HybridDeferredRenderer.cpp`
- Modify: `apps/client/CMakeLists.txt`
- Modify: `tests/engine_hybrid_deferred_renderer_test.cpp`

Interfaces:
- Consumes: `PerspectiveFrustum`
- Produces: static instance buffer and transparent marker instance buffer
- Produces: visible, culled and pass draw statistics

- [ ] Step 1: Write failing culling and draw count assertions

```cpp
EXPECT_EQ(1124U, stats.objectCount);
EXPECT_EQ(stats.objectCount, stats.visibleObjectCount + stats.culledObjectCount);
EXPECT_LT(stats.gBufferDrawCalls, 2240U);
EXPECT_EQ(1U, stats.transparentDrawCalls);
EXPECT_GT(stats.culledObjectCount, 0U);
```

- [ ] Step 2: Run RED

Run: `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug -R '^HybridDeferredRenderer\\.' --output-on-failure`

Expected: missing visibility fields or baseline-scale draw count.

- [ ] Step 3: Create reusable dynamic instance buffers

Allocate static capacity 1,000 and marker capacity 64 once. Each instance stores four world matrix rows. Map with `D3D11_MAP_WRITE_DISCARD`, copy visible matrices, unmap, and bind as `D3D11_INPUT_PER_INSTANCE_DATA`.

- [ ] Step 4: Build camera visible sets

Create frustum from the exact stress camera, FOV `XM_PIDIV4`, current aspect, near `0.1F`, far `200.0F`. Test static and character bounding spheres. Preserve deterministic input order in visible vectors.

- [ ] Step 5: Draw static geometry with instancing

Use `DrawIndexedInstanced(part.indexCount, visibleStaticCount, part.firstIndex, 0, 0)` for G-Buffer. Shadow uses the same full static buffer with all 1,000 matrices and one model-wide indexed instanced call.

- [ ] Step 6: Add transparent marker pass

Create alpha blend state and depth state with writes disabled. Draw 64 zone markers in one indexed instanced call after lighting. Unbind all G-Buffer and shadow SRVs before rebinding writable depth resources.

- [ ] Step 7: Run GREEN

Run: `./scripts/test.ps1`

Expected: hybrid WARP statistics meet assertions and all forward tests pass.

- [ ] Step 8: Commit

```powershell
git add assets/shaders/hybrid_geometry.hlsl assets/shaders/hybrid_shadow.hlsl assets/shaders/hybrid_transparent.hlsl engine/include/dxa/engine/HybridDeferredRenderer.hpp engine/src/windows/HybridDeferredRenderer.cpp apps/client/CMakeLists.txt tests/engine_hybrid_deferred_renderer_test.cpp
git commit -m "feat(renderer): instancing과 frustum culling 연결"
```

---

### Task 7: Engine, pass timer, runner integration

Files:
- Modify: `apps/client/src/main.cpp`
- Modify: `engine/include/dxa/engine/EngineApp.hpp`
- Modify: `engine/src/windows/EngineApp.cpp`
- Modify: `scripts/run_benchmark.ps1`
- Modify: `scripts/benchmark_common.ps1`
- Modify: `tests/engine_app_test.cpp`
- Modify: `tests/benchmark_runner_test.ps1`

Interfaces:
- Consumes: `RenderPath`, `HybridDeferredRenderer`, pass timer and schema 2
- Produces: CLI to engine path selection and runner render path validation

- [ ] Step 1: Write failing EngineApp hybrid report test

Run a 96×54 WARP benchmark for two frames with `RenderPath::HybridDeferred`. Assert summary JSON contains hybrid render path, GPU pass sample count two, visible object metrics and each pass draw count.

- [ ] Step 2: Write failing runner validation test

Fake a summary with `render_path = forward` while expected path is `hybrid-deferred`. Assert `Get-DxaBenchmarkValidationErrors` returns `Benchmark render path가 실행 인자와 일치하지 않습니다.`

- [ ] Step 3: Run RED

Run: `./scripts/build.ps1`

Expected: Engine options and helper do not accept render path.

- [ ] Step 4: Integrate renderer selection

`EngineRunOptions` carries `RenderPath`. Engine initializes only the selected renderer. The benchmark lambda calls `GpuFrameTimer::MarkPass` after each completed pass. Forward maps its total duration to forward.

- [ ] Step 5: Extend runner

Add:

```powershell
[ValidateSet('forward', 'hybrid-deferred')]
[string]$RenderPath = 'forward'
```

Pass `--render-path $RenderPath`, record it in environment JSON, and validate it against summary.

- [ ] Step 6: Run GREEN

Run: `./scripts/test.ps1`

Expected: EngineApp hybrid report, runner path mismatch and all existing tests pass.

- [ ] Step 7: Commit

```powershell
git add apps/client/src/main.cpp engine/include/dxa/engine/EngineApp.hpp engine/src/windows/EngineApp.cpp scripts/run_benchmark.ps1 scripts/benchmark_common.ps1 tests/engine_app_test.cpp tests/benchmark_runner_test.ps1
git commit -m "feat(client): 하이브리드 렌더 경로 실행 연결"
```

---

### Task 8: RTX comparison, documents, review and PR

Files:
- Create: `scripts/compare_benchmarks.ps1`
- Create: `tests/benchmark_comparison_test.ps1`
- Create: `docs/adr/0003-hybrid-deferred-rendering.md`
- Create: `docs/devlog/2026-08-23-hybrid-deferred.md`
- Create: `docs/benchmarks/hybrid-deferred/` 아래 runner가 만든 timestamp와 commit 기반 실행 디렉터리
- Modify: `README.md`
- Modify: `docs/PROJECT_PLAN.md`
- Modify: `docs/benchmarks/README.md`
- Modify: `tests/CMakeLists.txt`

Interfaces:
- Consumes: forward schema 1 raw and hybrid schema 2 raw
- Produces: validated comparison JSON and Korean problem-solving record

- [ ] Step 1: Write failing comparison validation test

Create two temporary summary JSON files. Matching seed, resolution and adapter must produce `comparison.json`. A seed mismatch must throw `Benchmark seed가 일치하지 않습니다.` and leave no comparison file.

- [ ] Step 2: Run comparison RED

Run: `pwsh -NoProfile -File tests/benchmark_comparison_test.ps1 -RepositoryRoot .`

Expected: failure because `scripts/compare_benchmarks.ps1` does not exist.

- [ ] Step 3: Implement and register comparison script

Read both summary files with `ConvertFrom-Json`. The locked schema 1 forward run has no `render_path`, so schema version 1 is accepted as forward only. Schema version 2 forward runs must say `forward`, and the hybrid run must say `hybrid-deferred`. Require equal seed, width, height and adapter. Calculate `((hybrid - forward) / forward) * 100` for CPU P95, GPU total P95, draw calls and working set. For schema 1 use `gpu_forward_ms` as GPU total. Write UTF-8 JSON only after all validations pass.

Run: `pwsh -NoProfile -File tests/benchmark_comparison_test.ps1 -RepositoryRoot .`

Expected: pass. Register it as `BenchmarkComparison.ValidatesComparableRuns` in CTest.

- [ ] Step 4: Run full local verification

Run: `./scripts/build.ps1`

Run: `./scripts/test.ps1`

Run: `./scripts/build.ps1 -Preset windows-msvc-release`

Expected: Windows Debug and Release pass, CTest has zero failures.

- [ ] Step 5: Run Linux warning build

Compile `BenchmarkReport.cpp`, `StressScene.cpp` and `PerspectiveFrustum.cpp` in `gcc:13` with `-Werror` flags from Task 2.

Expected: all three translation units compile.

- [ ] Step 6: Commit code before measurement

Commit `scripts/compare_benchmarks.ps1`, its test and CMake registration. Require clean status and capture the commit SHA. Do not run the official benchmark from a dirty tree.

- [ ] Step 7: Run RTX hybrid benchmark

```powershell
./scripts/run_benchmark.ps1 `
  -RenderPath hybrid-deferred `
  -OutputRoot docs/benchmarks/hybrid-deferred
```

Expected: RTX 3050 Ti, 1920×1080, 600 GPU total samples and no missing pass samples.

- [ ] Step 8: Compare against the locked forward run

```powershell
$hybridRun = Get-ChildItem docs/benchmarks/hybrid-deferred -Directory |
  Sort-Object Name |
  Select-Object -Last 1
./scripts/compare_benchmarks.ps1 `
  -ForwardRun docs/benchmarks/forward-baseline/20260823-033736-80988ef7-seed20260823 `
  -HybridRun $hybridRun.FullName
```

The script validates seed, resolution and adapter before writing `comparison.json`. It reports percent changes for CPU P95, GPU total P95, draw calls and working set without hiding regressions.

- [ ] Step 9: Write ADR and devlog from actual values

Use situation, reproduction, observation, alternatives, implementation, result and limits. Record shadow, G-Buffer, lighting and transparent GPU P95. State whether each portfolio case improved or regressed.

- [ ] Step 10: Update project status and commit raw evidence

```powershell
git add README.md docs/PROJECT_PLAN.md docs/adr/0003-hybrid-deferred-rendering.md docs/devlog/2026-08-23-hybrid-deferred.md docs/benchmarks/README.md docs/benchmarks/hybrid-deferred
git commit -m "docs(benchmark): 하이브리드 렌더링 비교 원본 기록"
```

- [ ] Step 11: Run code review and apply verified findings

Review from merge-base `9801635`. Inspect correctness, tests, maintainability, performance, reliability and silent-pass benchmark risks. Add regression tests before fixes and commit review changes separately.

- [ ] Step 12: Push and open the 5주차 PR

Push `feat/hybrid-deferred`, create a merge-commit PR against `main`, and monitor Windows and Ubuntu CI until it looks merge-ready. Do not merge without a new user instruction.
