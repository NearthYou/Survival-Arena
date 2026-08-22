# 범용 모델을 런타임 파일로 바꾸고 GPU에서 움직이기

## 상황

첫 DX11 화면은 렌더러 안에 들어 있는 큐브 정점만 그렸다. 파일을 읽는 코드가 없었고 머티리얼, 텍스처, 뼈 애니메이션도 없었다. 이번 목표는 런타임에 Assimp를 남기지 않고 CC0 캐릭터 한 개와 바닥 한 개를 같은 WARP 테스트에서 그리는 것이었다.

## 재현

기준 branch의 테스트는 27개였다. 에셋 포맷 테스트를 먼저 추가했을 때 `AssetFile.hpp`가 없어서 빌드가 실패했다. importer, texture cooker, CLI, palette 재생, asset scene 검증도 각각 필요한 API가 없는 상태에서 RED를 확인했다.

사용한 원본과 SHA-256은 `THIRD_PARTY_ASSETS.md`에 기록했다. Quaternius 캐릭터 FBX는 3,392,812바이트였고 Kenney Prototype Kit ZIP은 2,961,396바이트였다.

## 관찰

Assimp 구조를 그대로 파일에 쓰면 compiler padding, 포인터, endian에 종속된다. 그래서 정점과 문자열, 행렬 개수를 직접 little-endian으로 기록하고 최대 정점, 인덱스, 관절, 문자열 크기를 읽기 전에 검사했다.

Quaternius 캐릭터는 7,669 정점, 22,560 인덱스, 25 관절, 22 애니메이션으로 읽혔다. 상위 4개보다 많은 bone weight는 정규화 전에 잘라냈다. 애니메이션 node를 30Hz로 평가한 결과는 1,641,393바이트 `.dxam` 파일이 됐다.

텍스처 첫 테스트는 `DirectX::LoadFromWICFile failed with HRESULT 0x80004002`로 실패했다. BMP 헤더를 바꾸기 전에 DirectXTex 구현을 확인했다. WIC factory 생성 실패가 `E_NOINTERFACE`로 바뀌어 반환되는 경로였고, 공식 도구도 시작 시 `CoInitializeEx`를 호출했다.

테스트마다 COM을 초기화하고 해제하자 첫 테스트는 통과했지만 다음 테스트에서 access violation이 났다. DirectXTex가 WIC factory를 전역으로 보관하는데 첫 테스트가 apartment를 먼저 닫은 것이 원인이었다. 테스트와 CLI 모두 프로세스 전체에 COM apartment 하나를 유지하도록 바꿨다.

512×512 Kenney atlas를 기본 BC7 설정으로 변환하는 데 약 368초가 걸렸다. 프로세스는 그동안 한 코어를 계속 사용했고 결과 DDS는 349,700바이트였다. 빠른 encoder와 품질 비교는 현재 작업에 섞지 않았다.

## 가설과 대안

범용 포맷을 런타임에서 직접 읽는 방법은 첫 구현이 짧다. 대신 서버와 클라이언트 배포에 Assimp가 남고, 형식별 차이가 실행 중 오류가 된다.

애니메이션 key를 보존해 런타임에서 보간하는 방법도 검토했다. clip blending에는 유리하지만 첫 장면에서는 반복 재생만 필요했다. 이번에는 node 계층을 오프라인에서 평가한 최종 bone palette를 저장하고 vertex shader가 최대 4개 weight를 합성하도록 했다.

## 구현

`dxa_asset_tool model`은 Assimp 장면을 `.dxam`으로 저장한다. 정적 메시와 skinned mesh가 같은 vertex 구조를 사용하며 정적 정점은 weight 합이 0이다.

`dxa_asset_tool texture`는 WIC 입력을 sRGB로 읽고 mip 1×1까지 생성한 다음 BC7 DDS로 저장한다. 클라이언트는 DirectXTK로 DDS를 읽는다.

`AssetSceneRenderer`는 Quaternius 캐릭터와 Kenney 바닥을 각각 vertex와 index buffer로 만든다. 캐릭터는 첫 clip의 palette를 b1 상수 버퍼에 올리고 바닥 머티리얼은 `colormap.dds`를 사용한다. 기존 큐브 renderer는 asset root가 없는 엔진 테스트를 위해 남겼다.

병합 전 검토에서는 weight가 0인 슬롯의 joint index가 범위 검사를 건너뛰는 문제를 찾았다. shader는 weight가 0이어도 네 matrix를 모두 읽으므로 직접 만든 asset에서 범위 밖 접근이 가능했다. 모든 skinned joint index를 검사하도록 바꿨다. 긴 clip은 65,536 sample을 넘기기 전에 거부하고, texture 이름에는 부모 경로나 absolute path를 넣을 수 없게 했다.

## 검증

```text
Windows Debug 빌드: 성공
CTest: 53/53 통과
단일 프로세스 GoogleTest: 47/47 통과
이름별 client 검사: 6/6 통과
WARP 320×180 asset scene 3프레임: 0.24초, 종료 코드 0
RTX 하드웨어 960×540 asset scene 240프레임: 종료 코드 0
Quaternius 변환: 7,669 정점, 25 관절, 22 애니메이션
Kenney DDS: 512×512, BC7 sRGB, mip 10단계
```

CI checkout은 Git LFS 파일을 내려받도록 설정했다. 배포 검사는 원본과 실행 디렉터리의 SHA-256을 비교하고 최소 크기보다 작으면 pointer 파일로 판단해 실패한다.

## 남은 한계

- palette는 이전 30Hz sample을 선택하며 sample 사이를 보간하지 않는다.
- animation blending, root motion, clip 선택 UI가 없다.
- normal map, metallic, roughness와 alpha material을 처리하지 않는다.
- 기본 BC7 변환 368초는 반복 작업에 느리다. 빠른 설정은 품질 비교 뒤 결정해야 한다.
- pixel 검증은 장면에 무언가 그려졌는지만 확인하며 캐릭터 pose의 정답 이미지를 비교하지 않는다.

4주차 기준 장면을 만들 때는 cooked 파일을 고정하고 같은 camera path와 seed를 사용한다. 에셋 변환 결과가 바뀌면 기준 측정 전에 SHA를 다시 기록한다.
