# ADR 0002: 프레임 측정 중 GPU query를 기다리지 않음

- 상태: 채택
- 날짜: 2026-08-23

## 상황

포워드와 이후 렌더링 경로를 비교하려면 CPU 제출 시간과 GPU 패스 시간을 같은 프레임에서 얻어야 한다. GPU timestamp 결과를 만든 직후 `GetData`로 기다리면 측정 코드가 CPU 프레임 시간을 늘리고, 비교 대상마다 대기 시간이 달라질 수 있다.

DX11 timestamp는 GPU 주파수와 함께 해석해야 한다. 측정 도중 주파수가 불연속적으로 바뀐 프레임은 정상 값처럼 쓰면 안 된다.

## 결정

CPU 시간은 clear 직전부터 vsync 없는 present 반환까지 `steady_clock`으로 잰다. GPU 시간은 포워드 장면 제출 직전과 직후의 `D3D11_QUERY_TIMESTAMP` 차이로 계산한다. 각 프레임을 `D3D11_QUERY_TIMESTAMP_DISJOINT` 범위로 감싸고 `Disjoint`가 참인 결과는 비워 둔다.

실행 중 `GetData`는 `D3D11_ASYNC_GETDATA_DONOTFLUSH`로 이미 끝난 query만 확인한다. 측정 프레임 수만큼 슬롯을 실행 전에 만들기 때문에 아직 끝나지 않은 슬롯을 재사용하지 않는다. 모든 프레임을 보낸 뒤 한 번 flush하고 최대 2초 동안 남은 결과를 회수한다.

측정 원본은 프레임별 CSV와 최근접 순위 P50, P95, P99를 담은 JSON으로 나눈다. 실행 디렉터리가 이미 있으면 실패하며 덮어쓰지 않는다. commit SHA, seed, 해상도, 명령과 실행 환경을 함께 저장한다.

## 비교한 대안

프레임마다 query가 끝날 때까지 기다리는 방법은 구현이 단순하지만 CPU 시간에 GPU 대기가 섞인다. 이 값은 렌더 제출 비용이 아니라 동기화 비용까지 포함하므로 기준선으로 사용하지 않았다.

고정 16슬롯 순환 풀은 WARP와 짧은 실행에서는 동작했다. 실제 RTX 3050 Ti 600프레임 실행에서는 302개 query 시작이 거부됐다. 풀 크기를 임의로 32나 64로 늘리면 같은 문제가 다른 환경에서 다시 생길 수 있어 측정 프레임 수를 경계로 삼았다. 최대 측정 프레임은 query 객체 수를 제한하기 위해 10,000개로 둔다.

누락된 GPU 프레임만 제외해 통계를 내는 방법은 결과가 좋은 프레임 쪽으로 치우칠 수 있다. CSV에는 빈 값으로 남기되 공식 runner는 누락이 한 개라도 있으면 실행을 실패 처리한다.

## 결과

수정 전 1920×1080 120프레임 진단은 시작 거부 17, 정상 해석 103이었다. 프레임 수만큼 슬롯을 만든 뒤에는 시작 거부 0, 정상 해석 120이 됐다. 이어서 측정한 600프레임은 정상 해석 600, disjoint 0, 종료 미해석 0이었다.

query 생성 시간과 메모리는 준비 구간 전에 발생하므로 프레임 시간에는 들어가지 않는다. 다만 측정 프레임 수가 커질수록 query 객체도 늘어난다. 장시간 안정성 검사는 이 벤치마크가 아니라 별도 soak 시나리오로 확인한다.

## 참고

- [D3D11 query 종류](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_query)
- [timestamp disjoint 결과](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_query_data_timestamp_disjoint)
- [ID3D11DeviceContext::GetData](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-getdata)
