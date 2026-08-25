# Interest-full 사전 실행 실패

이 디렉터리는 공식 비교 결과가 아니다.

- 증상: DX11 client가 welcome 직후 종료했고 `protocol_errors=1`을 남겼다.
- 원인: runner가 game server에는 `interest-full`을 전달했지만 DX11 client에는 replication mode를 전달하지 않았다. client는 기본값인 `full-state`를 기대했다.
- 조치: server와 DX11 client 명령 양쪽에 같은 `--replication-mode`를 전달했다.
- 회귀: `NetworkLoadRunner` 테스트에서 runner source의 mode 전달 지점을 두 곳 이상 요구한다.
- 수정 commit: `01ae1278a5f304dfd83e289f671a01f1095489f3`

client와 bot 원본 로그는 실패 재현 자료로 그대로 보존했다.
