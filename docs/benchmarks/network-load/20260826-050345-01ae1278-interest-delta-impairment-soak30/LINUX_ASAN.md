# Ubuntu 24.04 ASan 검증

## 결과

- source 내용 기준 commit: `442f115dd6fa9a01cac8fe4dfc67d2875f132526`
- compiler: GCC 13.3.0
- build: `-Werror`, AddressSanitizer, UndefinedBehaviorSanitizer
- 반복 대상: `GameServerIntegration.PlayCoordinatorReportsEverySession`
- 참가자: headless client 1개, bot session 23개
- 실행 시간: 1800초
- 완료 반복: 1817회
- exit code: 0
- build negative marker: 0
- runtime negative marker: 0

## 빌드 중 확인한 문제

첫 build는 `AssetFile.cpp`의 16비트 조합 결과와 `SnapshotReplicator.cpp`의 aggregate 생략 초기화를 GCC가 경고로 거부했다. 두 번째 build에서는 새 option 필드를 생략한 test aggregate 두 곳이 같은 기준에 걸렸다.

명시적 변환과 완전 초기화로 고친 뒤 Linux build가 통과했다. 이 변경은 `442f115`에 저장했다. 그 다음 각 테스트 process가 정상 종료되도록 반복 실행해 매번 leak 검사가 수행되게 했다.

`linux-asan-build.log`, `linux-asan.log`, `linux-asan-summary.json`이 원본이다.
