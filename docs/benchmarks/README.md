# 벤치마크 원본 관리

포워드 기준선은 `scripts/run_benchmark.ps1`로 실행한다. 이 스크립트는 깨끗한 commit만 허용하며 기존 실행 디렉터리를 덮어쓰지 않는다.

```powershell
.\scripts\run_benchmark.ps1
```

기본 조건은 1920×1080, seed `20260823`, 준비 120프레임, 측정 600프레임이다. 장면에는 캐릭터 24명, AI 100개, 정적 객체 1,000개, 동적 포인트 광원 32개가 들어간다. 인스턴싱, 컬링, 디퍼드 조명은 적용하지 않은 상태다.

각 실행 디렉터리는 다음 파일을 가진다.

- `frames.csv`: 프레임별 CPU 시간, GPU 포워드 시간, 드로우콜, 삼각형, 객체 수, working set
- `summary.json`: 최근접 순위 방식의 P50, P95, P99와 실행 인자
- `environment.json`: commit SHA, 빌드 구성, OS, CPU, GPU와 드라이버

CPU 시간은 프레임 clear 직전부터 vsync 없는 present 반환까지 측정한다. GPU 시간은 포워드 장면 제출 전후 timestamp query로 측정한다. 실행 중에는 query 결과를 기다리지 않고 오래된 슬롯만 확인하며, disjoint이거나 제한 시간 안에 해석되지 않은 값은 빈 셀로 남긴다.

Debug 또는 WARP 결과는 동작 검증일 뿐 포트폴리오 수치로 사용하지 않는다. 기준선 원본을 수정하거나 좋은 프레임만 골라내지 않는다.
