# Full-state 사전 검증

이 디렉터리는 공식 네 mode 비교 전에 runner의 실제 production 경로를 확인한 자료다.

- commit: `703ddf9d7b4e733c9fc493e96e674dcf3360c7c9`
- 결과: Match 1, winner 4, tick 17430, reason 1
- 참가자 평균 수신량: 66.216112KiB/s
- 판정: 실행과 집계는 통과했지만 64KiB/s 목표는 미달

다음 interest-full 실행에서 DX11 client mode 전달 누락이 발견돼 runner를 수정했다. 공식 비교는 수정 뒤 commit `01ae1278a5f304dfd83e289f671a01f1095489f3`의 네 run만 사용한다.
