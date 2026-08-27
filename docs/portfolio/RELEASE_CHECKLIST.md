# 공개 준비 체크리스트

구조와 수치의 기준 코드는 `884e5e70d68d9fcf9dfe5638d97e06623da154c2`이며 최종 상태 편집 전 Task 6 검증 HEAD는 `0658869e5cbabe8479be201b1824177dc907888c`다. 상태 원본은 [release-status.json](release-status.json)이다. `verified`는 현재 요구를 직접 지지하는 원본이 있다는 뜻이고, `partial`은 역사적 또는 일부 local 원본은 있지만 현재 공개 후보의 플랫폼 및 hosted 검증 전체가 아니라는 뜻이다. `missing`은 산출물이 없고, `blocked`는 계정, 설치, 결제, 외부 승인 또는 현재 검증 환경의 복구 없이는 확인할 수 없는 상태다.

## 현재 상태

| 항목 | 상태 | 근거와 경계 |
| --- | --- | --- |
| 역사적 24인 성능 및 복제 지표 | `verified` | [24인 replication 비교](../benchmarks/network-load/20260826-01ae1278-COMPARISON.md)에 채택 수치와 목표 미달을 함께 남겼다. 현재 문서 HEAD 재측정은 아니다. |
| 역사적 30분 soak | `verified` | [Windows 및 Linux 요약](../benchmarks/network-load/20260826-050345-01ae1278-interest-delta-impairment-soak30/RESULT.md)과 [Linux sanitizer 기록](../benchmarks/network-load/20260826-050345-01ae1278-interest-delta-impairment-soak30/LINUX_ASAN.md)이 있다. 두 환경의 evidence commit은 같지 않다. |
| 코드 및 자산 라이선스 목록 | `verified` | [코드 라이선스](../../LICENSE), [외부 자산 목록](../../THIRD_PARTY_ASSETS.md), [LFS 규칙](../../.gitattributes), [dependency manifest](../../vcpkg.json)가 있다. |
| LFS object 실제 가용성 | `verified` | 2026-08-28에 HEAD `0658869e5cbabe8479be201b1824177dc907888c`에서 세 runtime asset의 committed canonical pointer, object-store와 hydrated worktree SHA-256 및 size를 production verifier로 read-only 대조했다. `git lfs fsck`는 호출하지 않았다. |
| 현재 문서 HEAD의 fresh Windows 및 Linux build | `partial` | 같은 HEAD의 Windows Debug build, CTest 582/582와 MSVC Release client 및 server build는 통과했다. Ubuntu 24.04 current-HEAD build는 Docker Desktop 4.81의 stale `dockerInference` socket으로 daemon이 종료되고 기존 Ubuntu WSL distro도 없어 실행하지 못했다. 문서 branch hosted CI도 없다. |
| WARP 및 RTX 시각 산출물 | `partial` | [RTX 기준 기록](../devlog/2026-08-23-forward-baseline.md)과 WARP 자동 검증은 있지만 공개용 실제 화면 캡처 묶음은 없다. |
| JSON 및 HTML 구조 다이어그램 네 개 | `verified` | [다이어그램 index](../diagrams/index.html)의 네 JSON 원본과 HTML이 기준 SHA 및 deterministic 생성 검사를 통과한다. |
| clang-uml AST 클래스 다이어그램 | `verified` | clang-uml 0.6.3의 실제 AST JSON과 deterministic HTML을 기준 commit에서 생성했다. [generation manifest](../diagrams/class/manifest.json)와 snapshot 검증 결과는 engine class 9개 및 relationship 5개, network class 9개 및 relationship 1개다. |
| 18쪽에서 22쪽 포트폴리오 PDF | `missing` | 렌더 및 링크 검수를 마친 PDF가 없다. |
| 실제 로컬 데모 영상 | `missing` | 3분 내외 실제 플레이 영상이 없다. |
| AWS 외부 접속 검증 | `blocked` | [AWS 실행 전 확인표](../../deploy/AWS_PRECHECK.md)는 있지만 resource는 만들지 않았다. 따라서 외부 성능, 종료와 잔여 비용도 검증하지 않았으며 cleanup 완료로 표시하지 않는다. |
| 저장소 공개 상태 | `blocked` | 공개 상태를 외부에서 확인하거나 변경하지 않았다. 공개 승인이 필요하다. |
| `v0.1.0` 태그 | `missing` | 공개 금지 조건과 승인이 남아 태그를 만들지 않았다. |

## Task 6 로컬 검증

2026-08-27부터 2026-08-28까지 HEAD `0658869e5cbabe8479be201b1824177dc907888c`에서 Node.js v24.16.0, MSVC 19.44.35228.0, Windows SDK 10.0.26100.0, CMake 및 CTest 3.31.6-msvc6로 다음을 확인했다.

| 명령 | 결과 | 소요 시간 |
| --- | --- | ---: |
| `node --test tests/portfolio_evidence_test.mjs tests/portfolio_diagram_test.mjs tests/portfolio_class_diagram_test.mjs` | 99/99 | 49.027초 |
| `node scripts/portfolio/render-diagrams.mjs --root . --check` | source 6개 검증, HTML 7개 unchanged | 7.073초 |
| `node scripts/portfolio/verify-all.mjs --root .` | case 5개, release item 13개, Markdown 77개 | 10.116초 |
| Windows Debug configure 및 `.\scripts\build.ps1 -Preset windows-msvc-debug` | `/WX` build 성공 | 12.420초 및 423.705초 |
| `ctest --test-dir out/build/windows-msvc-vs-debug --build-config Debug --output-on-failure` | 582/582 | 176.09초 |
| Windows Release configure 및 `.\scripts\build.ps1 -Preset windows-msvc-release` | client, lobby server, game server build 성공 | 11.101초 및 206.207초 |
| Gitleaks 8.30.1 전체 이력 | 217 commits, leak 0 | 1.043초 |

첫 CTest는 `PortfolioDiagrams.RendersDeterministically`가 invocation cwd에 의존해 581/582로 실패했다. validator root를 이미 계산한 repository root로 고정한 뒤 direct Node 15/15, focused CTest 1/1과 전체 582/582를 다시 통과했다.

Ubuntu 24.04 build는 성공으로 기록하지 않는다. Docker Desktop 4.81은 0바이트 `C:\Users\siwon\AppData\Local\Docker\run\dockerInference` reparse point를 제거하지 못해 backend가 종료됐다. Docker process 0, exact parent, filename, length와 reparse attribute를 확인했지만 승인된 PowerShell 삭제는 실행 정책이 차단했고 same-directory rename과 exact patch 삭제도 OS error 1920으로 실패했다. `EnableDockerAI`를 `false`로 바꾼 단일-field 복구도 같은 오류를 재현해 즉시 `true`로 원복했으며 settings 파일 SHA-256은 원본 `30a8d70713ccd6864a4347483c005e9b5a06baa74f184e7a77ae338cb8e709f7`과 다시 일치한다. WSL에는 `docker-desktop`만 있고 Ubuntu distro는 없었다.

별도 public checkout에서 알려진 historical benchmark blob 8개의 `should have been pointers` 경고는 그대로 남는다. 이 경고 대상과 raw benchmark는 수정하지 않았고, 위 runtime asset 3개의 proof와 구분한다.

## 공개 전 남은 조건

다음 항목을 모두 실제 산출물과 실행 기록으로 닫기 전에는 저장소 공개나 `v0.1.0` 생성을 완료로 표시하지 않는다.

1. 실제 게임 화면과 구조 그림을 사용한 18쪽에서 22쪽 PDF를 렌더하고 모든 링크를 검수한다.
2. 실제 로컬 플레이를 담은 3분 내외 데모 영상을 만든다.
3. Docker 또는 독립 Ubuntu 24.04 환경을 복구한 뒤 최종 공개 후보 HEAD의 Windows 및 Linux build와 test, hosted CI를 새로 실행한다.
4. 공개용 WARP 및 RTX 화면 캡처를 PDF와 영상 산출물에서 검수한다.
5. AWS 외부 테스트를 선택하면 생성 전 account, 가격과 보안 그룹을 확인하고 종료 뒤 실제 resource와 잔여 비용을 검증한다.
6. 저장소 공개 승인과 실제 visibility 확인 뒤에만 `v0.1.0`을 만든다.

현재 알려진 기술 경계는 [통합 한계](LIMITATIONS.md)에 따로 유지한다. 전체 문서 계약은 저장소 루트에서 다음 명령으로 확인한다.

```powershell
node scripts/portfolio/verify-all.mjs --root .
```
