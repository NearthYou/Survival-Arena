# 측정 데이터

현재 게임의 렌더링, 길찾기와 쿼드트리 결과는 `engine-diagnostics/2026-09-07/`에 있다. [측정 조건과 결과](../implementation/engine-measurements.md)를 함께 볼 수 있다.

| 파일 | 내용 |
| --- | --- |
| `diagnostics-render.csv` | 프레임 시간, 그리기 호출 수와 인덱스 수 |
| `diagnostics-paths.csv` | 고정 경로별 탐색 시간과 경로 지점 수 |
| `diagnostics-quadtree.csv` | 충돌 후보 수, 전체 쌍 검사, 트리 구축과 충돌 처리 시간 |
| `summary.json` | 실행 조건과 집계 결과 |

나머지 폴더에는 이전 렌더러와 서버의 측정 데이터를 보관한다. 현재 게임의 성능 수치와 구분한다.
