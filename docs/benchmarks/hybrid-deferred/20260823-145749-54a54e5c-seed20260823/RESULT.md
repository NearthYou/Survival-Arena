# Hybrid deferred benchmark 결과

## 실행 조건

- 상태: 통과
- commit: `54a54e5cac60cbc2f8d025d0500f0b7ac70be57e`
- branch: `feat/hybrid-deferred`
- build: MSVC Release
- adapter: NVIDIA GeForce RTX 3050 Ti Laptop GPU
- 해상도: 1920×1080
- seed: `20260823`
- 준비 frame: 120
- 측정 frame: 600
- render path: `hybrid-deferred`

실행 전 작업 트리는 깨끗했다. GPU query는 시작 거부 0, 정상 해석 600, 폐기 0, 종료 미해석 0이었다. total, shadow, G-Buffer, lighting, transparent sample은 각각 600개다.

## 결과

| 항목 | P50 | P95 | P99 | 최대 |
| --- | ---: | ---: | ---: | ---: |
| CPU frame | 1.4964ms | 2.1695ms | 2.3639ms | 3.3053ms |
| GPU total | 1.5104ms | 2.174976ms | 2.239488ms | 2.445312ms |
| GPU shadow | 0.391168ms | 0.733184ms | 0.766976ms | 0.786432ms |
| GPU G-Buffer | 0.480256ms | 0.800768ms | 0.888832ms | 1.16736ms |
| GPU lighting | 0.627712ms | 0.970752ms | 1.015808ms | 1.329152ms |
| GPU transparent | 0.002048ms | 0.003072ms | 0.003072ms | 0.008192ms |

Draw calls P50은 1,148회다. pass별 P50은 shadow 125회, G-Buffer 1,021회, lighting 1회, transparent 1회다. 객체 수는 1,124개로 유지했고 visible object P50은 1,009개, culled object P50은 114개였다.

forward 기준선과 비교하면 CPU frame P95 44.693706%, GPU total P95 44.016869%, draw calls P50 48.75%, working set P95 30.119342% 감소했다.

## 원본

- [frames.csv](frames.csv): 600개 frame 원본
- [summary.json](summary.json): percentile과 pass별 집계
- [environment.json](environment.json): 실행 환경과 검증 결과
- [comparison.json](comparison.json): 잠긴 forward 기준선과의 비교

pass별 P95는 각기 다른 frame에서 나올 수 있으므로 서로 더하지 않는다. working set은 GPU resource 크기만 나타내는 값이 아니므로 메모리 최적화의 단독 근거로 사용하지 않는다.
