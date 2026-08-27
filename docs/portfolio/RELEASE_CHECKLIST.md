# 공개 준비 체크리스트

기준 코드는 `884e5e70d68d9fcf9dfe5638d97e06623da154c2`이며 상태 원본은 [release-status.json](release-status.json)이다. `verified`는 현재 요구를 직접 지지하는 원본이 있다는 뜻이고, `partial`은 역사적 원본은 있지만 현재 HEAD 또는 필요한 시각 산출물 전체를 검증하지 않았다는 뜻이다. `missing`은 산출물이 없고, `blocked`는 계정, 설치, 결제 또는 외부 승인 없이는 확인할 수 없는 상태다.

## 현재 상태

| 항목 | 상태 | 근거와 경계 |
| --- | --- | --- |
| 역사적 24인 성능 및 복제 지표 | `verified` | [24인 replication 비교](../benchmarks/network-load/20260826-01ae1278-COMPARISON.md)에 채택 수치와 목표 미달을 함께 남겼다. 현재 문서 HEAD 재측정은 아니다. |
| 역사적 30분 soak | `verified` | [Windows 및 Linux 요약](../benchmarks/network-load/20260826-050345-01ae1278-interest-delta-impairment-soak30/RESULT.md)과 [Linux sanitizer 기록](../benchmarks/network-load/20260826-050345-01ae1278-interest-delta-impairment-soak30/LINUX_ASAN.md)이 있다. 두 환경의 evidence commit은 같지 않다. |
| 코드 및 자산 라이선스 목록 | `verified` | [코드 라이선스](../../LICENSE), [외부 자산 목록](../../THIRD_PARTY_ASSETS.md), [LFS 규칙](../../.gitattributes), [dependency manifest](../../vcpkg.json)가 있다. |
| LFS object 실제 가용성 | `partial` | 현재 checkout의 세 runtime asset은 pointer가 아닌 파일이지만 공개 후보 checkout에서 dated `git lfs fsck`와 hydration proof를 아직 남기지 않았다. 라이선스 및 자산 목록 완료와 object availability를 같은 gate로 취급하지 않는다. |
| 현재 문서 HEAD의 fresh Windows 및 Linux build | `partial` | PR #11 head `e2aba12c670b288b596169b8115b1fef77d54068`의 hosted run `32935640972`에서 Windows와 Ubuntu가 성공했고 `884e5e70d68d9fcf9dfe5638d97e06623da154c2`로 main에 병합됐다. 이후 문서 커밋을 포함한 현재 HEAD hosted CI는 아직 없다. |
| WARP 및 RTX 시각 산출물 | `partial` | [RTX 기준 기록](../devlog/2026-08-23-forward-baseline.md)과 WARP 자동 검증은 있지만 공개용 실제 화면 캡처 묶음은 없다. |
| JSON 및 HTML 구조 다이어그램 네 개 | `verified` | [다이어그램 index](../diagrams/index.html)의 네 JSON 원본과 HTML이 기준 SHA 및 deterministic 생성 검사를 통과한다. |
| clang-uml AST 클래스 다이어그램 | `missing` | clang-uml 0.6.3의 실제 AST JSON과 HTML이 없다. 수동 class 목록으로 완료 조건을 낮추지 않는다. |
| 18쪽에서 22쪽 포트폴리오 PDF | `missing` | 렌더 및 링크 검수를 마친 PDF가 없다. |
| 실제 로컬 데모 영상 | `missing` | 3분 내외 실제 플레이 영상이 없다. |
| AWS 외부 접속 검증 | `blocked` | [AWS 실행 전 확인표](../../deploy/AWS_PRECHECK.md)는 있지만 resource는 만들지 않았다. 따라서 외부 성능, 종료와 잔여 비용도 검증하지 않았으며 cleanup 완료로 표시하지 않는다. |
| 저장소 공개 상태 | `blocked` | 공개 상태를 외부에서 확인하거나 변경하지 않았다. 공개 승인이 필요하다. |
| `v0.1.0` 태그 | `missing` | 공개 금지 조건과 승인이 남아 태그를 만들지 않았다. |

## 공개 전 남은 조건

다음 항목을 모두 실제 산출물과 실행 기록으로 닫기 전에는 저장소 공개나 `v0.1.0` 생성을 완료로 표시하지 않는다.

1. clang-uml 0.6.3과 현재 compilation database에서 engine 및 network AST class JSON과 HTML을 생성한다.
2. 실제 게임 화면과 구조 그림을 사용한 18쪽에서 22쪽 PDF를 렌더하고 모든 링크를 검수한다.
3. 실제 로컬 플레이를 담은 3분 내외 데모 영상을 만든다.
4. 공개 후보 HEAD에서 Windows 및 Linux build와 test를 새로 실행한다.
5. 공개 후보 checkout에서 `git lfs fsck`와 runtime asset hydration을 확인하고 dated proof를 남긴다.
6. AWS 외부 테스트를 선택하면 생성 전 account, 가격과 보안 그룹을 확인하고 종료 뒤 실제 resource와 잔여 비용을 검증한다.
7. 저장소 공개 승인과 실제 visibility 확인 뒤에만 `v0.1.0`을 만든다.

현재 알려진 기술 경계는 [통합 한계](LIMITATIONS.md)에 따로 유지한다. 전체 문서 계약은 저장소 루트에서 다음 명령으로 확인한다.

```powershell
node scripts/portfolio/verify-all.mjs --root .
```
