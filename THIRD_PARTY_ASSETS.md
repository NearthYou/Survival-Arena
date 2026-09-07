# 외부 리소스와 라이선스

## 게임 리소스

이터널 리턴의 이미지와 에셋 저작권은 ㈜님블뉴런 및 각 권리자에게 있다. 게임에서 추출한 원본 파일과 이를 포함한 실행 패키지는 이 저장소에서 배포하지 않는다. 팬키트는 [공식 배포처](https://drive.google.com/drive/folders/1bgW32L09YPpRgQKtH4C_TAd3Kr0N9Y90)의 [이용 정책](https://support.playeternalreturn.com/hc/ko/articles/49503976113177)을 따른다.

## 공개 샘플

아래 세 파일은 별도 렌더링 도구에서 사용하는 CC0 샘플이다.

| 파일 | 배포처 | 라이선스 |
| --- | --- | --- |
| `assets/runtime/characters/cyber-runner.dxam` | [Quaternius Cyberpunk Game Kit](https://quaternius.com/packs/cyberpunkgamekit.html) | CC0 1.0 |
| `assets/runtime/environment/prototype-floor.dxam` | [Kenney Prototype Kit](https://kenney.nl/assets/prototype-kit) | CC0 1.0 |
| `assets/runtime/environment/colormap.dds` | [Kenney Prototype Kit](https://kenney.nl/assets/prototype-kit) | CC0 1.0 |

FBX는 `dxa_asset_tool model`, 텍스처는 `dxa_asset_tool texture`로 변환했다. 모델은 정점, 본과 애니메이션을 `.dxam`에 저장하고, 텍스처는 DDS로 저장한다.

## 포함된 외부 소스

| 소스 | 저작권자 | 라이선스 고지 |
| --- | --- | --- |
| Dear ImGui | Omar Cornut | [MIT](licenses/imgui-LICENSE.txt) |
| ImGuizmo | Cedric Guillemet | [소스에 포함된 MIT 고지](game/Engine/ImGuizmo.h) |
| DirectXTK SimpleMath | Microsoft Corporation | [MIT](licenses/directxtk-LICENSE.txt) |
| TinyXML-2 | Lee Thomason | [소스에 포함된 zlib 고지](game/Engine/tinyxml2.h) |
| ImGui의 imstb 헤더 | Sean Barrett 및 기여자 | 각 헤더에 포함된 MIT / Public Domain 고지 |

외부 소스의 기존 저작권 문구는 유지한다. 프로젝트의 [MIT 라이선스](LICENSE)는 외부 리소스의 이용 조건을 대신하지 않는다.
