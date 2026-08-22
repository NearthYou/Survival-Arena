## Code Review Results

Scope: `main` merge-base `6e69409`에서 `ae6e617`까지, 39개 파일, 실행 코드 약 2,498줄

Intent: 범용 모델과 텍스처를 Windows 도구에서 cooked 파일로 만들고, CC0 캐릭터와 바닥을 DX11 GPU 스키닝 장면으로 실행한다. 기존 큐브 renderer와 Linux 서버 경계는 보존한다.

Mode: local report and verified fixes

Review lenses:

- correctness: 바이너리 범위, animation sample, shader input 추적
- testing: RED와 GREEN 증거, LFS 및 배포 guard의 거짓 통과 가능성
- maintainability: 새 에셋 경계와 renderer 결합도
- performance: importer와 BC7 가공 비용, 런타임 hot path
- reliability: 파일 I/O, COM 수명, 실패 전파
- adversarial: crafted asset, LFS pointer, path boundary 조합

### Applied

| # | File | Fix | Verification |
|---|---|---|---|
| 1 | `engine/src/assets/AssetFile.cpp` | zero-weight 슬롯을 포함한 모든 skinned joint index 범위 검사 | RED 후 대상 테스트 통과 |
| 2 | `apps/asset_tool/src/ModelImporter.cpp` | 65,536 sample 초과를 palette allocation 전에 거부 | 300초, 240Hz 입력 RED 후 통과 |
| 3 | `engine/src/assets/AssetFile.cpp` | texture 이름에서 부모 경로, absolute 경로 구성 문자, NUL 거부 | parent traversal RED 후 통과 |

Applied commit: `ae6e617 fix(asset): GPU 입력과 palette 범위 검증`

Validation: CTest 53/53, 이름별 client 검사 6/6, 단일 프로세스 GoogleTest 47/47, Windows Release 빌드 성공, Git LFS fsck 성공.

### Requirements Completeness

- met: Assimp 모델 변환
- met: DirectXTex mip 및 BC7 DDS 가공
- met: 머티리얼 색상과 texture 참조
- met: 30Hz animation palette와 GPU 스키닝
- met: Quaternius와 Kenney CC0 자산 최소 반입
- met: 원본 URL, 라이선스, SHA-256, 사용 파일 manifest

### Actionable Findings

현재 남은 actionable finding은 없다.

### Coverage

- 저장소 안에 적용 가능한 `AGENTS.md` 또는 `CLAUDE.md` 파일은 없었다. 대화에 제공된 branch, commit, 문서 표기 규칙은 별도로 확인했다.
- 외부 cross-model 검토는 사용자 금지 원칙에 따라 실행하지 않았다.
- 저장소 지침에 따라 독립 하위 reviewer를 파견하지 않았다. 따라서 reviewer 간 독립 corroboration은 없다.
- residual risk: 512×512 기본 BC7 가공이 약 368초 걸렸다.
- residual risk: cooked 파일의 aggregate animation matrix 수에는 파일 크기 외 별도 총량 budget이 없다.
- testing gap: pixel 검증은 pose 정답 이미지나 프레임 사이의 실제 화면 변화를 비교하지 않는다.

---

> Verdict: Ready to merge
>
> Reasoning: 로컬에서 확인된 세 결함은 수정됐고 계획한 3주차 요구사항이 모두 구현됐다. Windows와 Linux 원격 CI가 현재 LFS checkout과 조건부 vcpkg 의존성을 다시 검증해야 한다.

Actionable findings: none.
