# 5주차 하이브리드 디퍼드 렌더링 설계

## 목표

4주차 포워드 기준선을 바꾸지 않고 같은 스트레스 장면을 하이브리드 디퍼드 경로로 렌더링한다. 방향광 그림자, G-Buffer, fullscreen 조명, 투명 포워드 패스, 정적 GPU instancing과 camera frustum culling을 연결한다.

비교는 seed `20260823`, 프레임 기반 카메라, 1920×1080, 준비 120프레임과 측정 600프레임을 유지한다. CPU frame, GPU total과 pass별 시간, draw call, triangle, visible과 culled object, working set을 원본으로 보관한다.

## 범위 밖

- render graph 프레임워크
- clustered 또는 tiled lighting
- character GPU instancing과 palette texture
- cascaded shadow map
- SSAO, bloom, tone mapping
- 여러 투명 material과 정렬 시스템

이번 주에는 다음 비교에 필요한 최소 경계만 만든다. character는 camera frustum culling만 하고 기존 mesh part draw를 유지한다.

## 선택한 구조

기존 `AssetSceneRenderer`는 4주차 포워드 기준선으로 보존한다. 새 `HybridDeferredRenderer`가 같은 cooked asset과 `StressScene`을 별도로 읽는다. `EngineApp`은 `RenderPath`에 따라 renderer 하나만 초기화한다.

기존 renderer를 직접 확장하면 baseline 구현도 함께 바뀌어 비교가 어려워진다. 반대로 범용 render graph를 먼저 만들면 pass dependency와 resource lifetime 추상화가 이번 범위를 넘는다. 두 renderer의 asset upload 중복은 현재 한 프로세스에서 한 경로만 사용하므로 허용한다. 공통 경계가 실제로 안정된 뒤 별도 작업으로 합친다.

## 공개 계약

`RenderPath`는 `forward`와 `hybrid-deferred` 두 값만 가진다. client는 `--render-path`로 선택하며 benchmark metadata와 environment에 선택 값을 기록한다. 일반 실행 기본값은 `forward`로 유지한다.

`HybridDeferredConfig`는 output 크기, shadow map 크기와 stress seed를 받는다. renderer는 back buffer RTV를 인자로 받고 pass별 `RenderStatistics`를 반환한다. `GraphicsDevice`는 소유권을 넘기지 않는 back buffer RTV 접근자만 제공한다.

플랫폼 중립 `PerspectiveFrustum`은 camera eye와 target, vertical FOV, aspect, near와 far로 만들어진다. `IntersectsSphere`는 world-space bounding sphere가 camera frustum과 겹치는지만 반환한다. DirectXMath와 Win32에는 의존하지 않는다.

## 렌더링 순서

### Directional shadow

2048×2048 `DXGI_FORMAT_R32_TYPELESS` texture에 `DXGI_FORMAT_D32_FLOAT` DSV와 `DXGI_FORMAT_R32_FLOAT` SRV를 만든다. 방향광은 arena 전체를 덮는 orthographic view projection을 사용한다.

arena 안 전체 정적 instance는 한 번의 `DrawIndexedInstanced`로 제출한다. character는 material 분할 없이 model 전체 index buffer를 object당 한 번 그린다. shadow rasterizer는 depth bias를 사용한다.

### G-Buffer

같은 크기와 sample count의 render target 두 개와 depth target 하나를 동시에 bind한다.

- RT0: `DXGI_FORMAT_R8G8B8A8_UNORM`, albedo RGB와 roughness A
- RT1: `DXGI_FORMAT_R16G16_SNORM`, octahedral encoded world normal
- Depth resource: `DXGI_FORMAT_R24G8_TYPELESS`
- Depth DSV: `DXGI_FORMAT_D24_UNORM_S8_UINT`
- Depth SRV: `DXGI_FORMAT_R24_UNORM_X8_TYPELESS`

정적 mesh는 visible instance matrix buffer를 input slot 1에 bind한다. character는 기존 skin constant buffer와 object world matrix를 사용한다.

### Deferred lighting

back buffer에 fullscreen triangle 하나를 그린다. depth와 inverse view projection으로 world position을 복원한다. 방향광과 최대 32개 point light를 적용하고 shadow map을 comparison sample한다.

G-Buffer와 depth SRV는 다음 write pass 전에 명시적으로 unbind한다. 같은 subresource를 read와 write에 동시에 bind하지 않는다.

### Transparent forward

arena 경계를 나타내는 반투명 marker를 alpha blend로 그린다. depth test는 유지하고 depth write는 끈다. marker는 하나의 instance buffer와 `DrawIndexedInstanced` 한 번으로 제출한다.

## Culling과 instancing

camera frustum은 stress camera에서 매 프레임 계산한다. static instance와 character 각각의 world-space bounding sphere를 검사한다.

static visible matrix만 dynamic instance buffer에 복사한다. instance buffer capacity는 전체 static count로 한 번 할당하고 매 프레임 재사용한다. visible count가 0이면 draw를 호출하지 않는다.

character는 frustum 밖이면 G-Buffer draw를 건너뛴다. shadow pass는 camera가 아닌 light volume을 기준으로 하며 arena 안 object를 모두 포함한다.

## GPU 시간 계약

`GpuFrameTimer`는 frame start와 pass boundary timestamp를 저장한다. hybrid 경로는 shadow, G-Buffer, deferred lighting, transparent marker 순으로 marker를 남긴다. forward 경로는 기존 total 값과 같은 forward marker 하나를 남긴다.

disjoint frame은 모든 GPU pass 값을 비운다. 실행 중에는 `D3D11_ASYNC_GETDATA_DONOTFLUSH`로 완료된 query만 읽는다. 측정 frame 수만큼 slot을 미리 만든다는 4주차 결정을 유지한다.

## 보고서와 비교

schema version 2 CSV는 기존 열을 유지하고 다음 열을 추가한다.

- render_path
- gpu_shadow_ms
- gpu_gbuffer_ms
- gpu_lighting_ms
- gpu_transparent_ms
- shadow_draw_calls
- gbuffer_draw_calls
- lighting_draw_calls
- transparent_draw_calls
- visible_objects
- culled_objects

summary JSON은 pass별 P50, P95, P99와 render path를 기록한다. 4주차 schema version 1 원본은 수정하지 않는다.

비교 문서는 forward와 hybrid 실행의 commit, seed, adapter, 해상도가 맞을 때만 표를 만든다. 목표 미달과 악화 수치도 그대로 기록한다.

## 오류 처리

- output 크기 또는 shadow 크기가 0이면 초기화를 거부한다.
- G-Buffer, shadow view 또는 shader 생성 실패는 HRESULT와 operation을 포함해 중단한다.
- benchmark render path가 metadata와 다르면 runner가 실패한다.
- GPU marker 순서가 경로 계약과 다르면 해당 frame을 누락 처리하지 않고 프로그램 오류로 중단한다.
- 기존 실행 디렉터리는 계속 덮어쓰지 않는다.

## 테스트

- render path CLI 파싱과 잘못된 값 거부
- frustum의 near, far, side plane과 sphere 교차 경계
- 동일 camera에서 deterministic visible set
- pass marker 순서와 duration 계산
- schema 2 CSV와 JSON, pass 누락 처리
- WARP hybrid 96×54 frame의 object, culling, pass draw 통계와 non-clear pixel
- shadow, G-Buffer, depth, hybrid shader 배포 일치
- Windows 전체 CTest와 Linux 플랫폼 중립 build
- RTX 3050 Ti 1920×1080 Release 600 frame 비교

## 참고

- [DrawIndexedInstanced](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-drawindexedinstanced)
- [Depth-stencil 구성](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-depth-stencil)
- [OMSetRenderTargets](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-omsetrendertargets)
