## Code Review Results

Scope: `origin/main` merge-base `ee801c8`에서 `4cb26e8`까지, 39개 파일, 실행 코드 약 1,774줄

Intent: 고정 seed 포워드 스트레스 장면을 만들고 CPU, GPU, 드로우콜과 working set을 비동기로 측정한다. 24 캐릭터, AI 100개, 정적 객체 1,000개와 동적 광원 32개를 최적화 전 경로로 렌더링하고 실패 원본까지 보존한다.

Mode: local apply and verified fixes

Review lenses:

- correctness: CLI 경계, 프레임 범위, query 결과와 sample 연결, 실행 SHA 추적
- testing: WARP와 RTX 증거, PowerShell guard, 실패 경로의 false-pass 가능성
- maintainability: benchmark 모듈과 renderer, EngineApp의 책임 경계
- performance: 객체별 draw, query 슬롯, 측정 코드가 프레임 시간에 미치는 영향
- reliability: query timeout, 부분 파일, 시스템 환경 수집과 오류 전파
- adversarial: build 중 변경, hidden window, 검증 실패가 증거를 잃는 순서

### Applied

| # | File | Fix | Reviewer |
| --- | --- | --- | --- |
| 1 | `scripts/run_benchmark.ps1:102` (+helper, test) | summary 검증 전에 environment를 쓰고 `validation.status`와 오류 목록 보존 | reliability |
| 2 | `scripts/run_benchmark.ps1:53` (+helper, test) | Release build 뒤 Git snapshot 재검증으로 dirty tree와 HEAD 이동 거부 | correctness, adversarial |

Validation: PowerShell 3개 파일 구문 통과, 임시 Git 저장소에서 clean, dirty와 HEAD 이동 경로 통과, 의도한 adapter 불일치 실행에서 `validation.status=failed` 환경 파일 보존, BenchmarkRunner CTest 추가, 전체 CTest 74/74와 client 배포 검사 6/6 통과.

Committed: `63f3733 fix(review): 기준선 증거 경계 보강`

### Coverage

- 초기 fast pass에서 P0 또는 P1 후보는 없었다.
- mechanics는 finding 2개를 병합했고 malformed 또는 confidence suppression은 없었다.
- 저장소 안에 적용 가능한 `AGENTS.md` 또는 `CLAUDE.md` 파일은 없었다. 대화에 제공된 branch, commit과 문서 표기 규칙은 별도로 확인했다.
- 외부 cross-model 검토는 사용자 금지 원칙에 따라 실행하지 않았다.
- 저장소 지침에 따라 별도 reviewer와 validator를 파견하지 않았다. 각 관점은 현재 작업에서 순서대로 검사했으므로 독립 corroboration은 없다. confidence 승격은 제거하고 코드 인용과 실행 재현으로 두 finding을 검증했다.
- formal plan artifact는 발견되지 않아 settlement suppression은 평가하지 않았다. 대화에서 확정된 4주차 요구사항은 intent 검증에 사용했다.
- residual risk: 기본 600프레임은 검증됐지만 최대 10,000프레임에서 query 객체 30,000개 생성과 슬롯 순회 비용은 측정하지 않았다.
- residual risk: 채택한 실행은 hidden window여서 보이는 창의 presentation 비용과 기준 이미지 비교는 포함하지 않는다.
- testing gap: RTX readback은 clear 색 이외 픽셀을 확인하며 기준 이미지 비교는 하지 않는다.

### Actionable Findings

현재 남은 actionable finding은 없다.

---

### Verdict

> Verdict: Ready to merge
>
> Reasoning: 리뷰에서 확인한 두 증거 신뢰성 결함을 수정하고 새 PowerShell guard를 포함한 전체 CTest 74건을 통과했다. 포워드 기준선 원본과 실패 원본은 각각 고정돼 있다.
>
> Fix order: 완료. 남은 항목은 5주차 비교와 장기 부하에서 확인할 residual risk다.

Actionable findings: none.
