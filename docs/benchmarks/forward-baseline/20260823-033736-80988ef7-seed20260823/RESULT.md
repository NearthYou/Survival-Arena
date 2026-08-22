# 포워드 기준선 채택

이 실행을 4주차 포워드 기준선으로 채택한다. 원본 `frames.csv`, `summary.json`, `environment.json`은 수정하지 않았다.

## 조건

- commit: `80988ef71f21430fc6baabea7b3d5cb4beae71f0`
- build: MSVC Release
- adapter: NVIDIA GeForce RTX 3050 Ti Laptop GPU, driver 610.88
- CPU: AMD Ryzen 7 6800HS Creator Edition
- 해상도: 1920×1080
- seed: `20260823`
- 준비 120프레임, 측정 600프레임
- 캐릭터 24명, AI 100개, 정적 객체 1,000개, 동적 광원 32개

## 결과

| 항목 | P50 | P95 | P99 | 최대 |
| --- | ---: | ---: | ---: | ---: |
| CPU frame | 2.0068ms | 3.9227ms | 4.6404ms | 5.5797ms |
| GPU forward | 2.085888ms | 3.885056ms | 4.455424ms | 4.978688ms |
| working set | 237,809,664B | 238,190,592B | 238,190,592B | 238,190,592B |

GPU timestamp는 600개 모두 해석됐다. CPU와 GPU에서 16.7ms를 넘은 프레임은 0개였다. 매 프레임 2,240 draw calls, 944,480 triangles, 1,124 objects를 기록했다.

같은 RTX 경로의 1920×1080 3프레임 back buffer readback에서도 clear 색 이외의 픽셀이 확인됐다.

working set은 182,026,240바이트에서 시작해 238,190,592바이트로 끝났다. 마지막 100프레임의 증가는 217,088바이트였다. 이 실행은 짧은 렌더 기준선이므로 메모리 누수 판정에는 사용하지 않는다.

## 판정

P95 16.7ms 목표는 통과했다. 다만 아직 게임 로직, 서버 통신, UI, 그림자와 투명 패스가 없으므로 최종 클라이언트 성능을 뜻하지 않는다. 5주차의 디퍼드 조명, 인스턴싱과 컬링은 이 실행과 같은 seed와 카메라 경로로 비교한다.
