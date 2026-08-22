# 첫 엔진 코어를 PR 전에 깨뜨려 보기

## 상황

첫 DX11 프레임이 실행된 뒤 바로 PR을 열지 않았다. 종료 코드 0과 단위 테스트만으로는 자원 수명, 입력 edge, 실제 픽셀 출력까지 맞다고 보기 어려웠다. 정확성, 테스트, 유지보수성, 실패 경로를 나눠 현재 branch 전체를 다시 읽었다.

## 재현

검토 항목은 의견으로 바로 고치지 않고 각각 실패 조건을 먼저 만들었다.

- 자원 생성자가 예외를 던진 다음 생성하면 index가 0이 아니라 1로 건너뛰었다.
- 같은 프레임 안에 key-down과 key-up을 연속 적용하면 pressed와 released가 모두 false였다.
- 시스템 키 기본 처리를 제거한 창에서는 합성한 Alt+F4가 종료로 이어지지 않았다.
- `--hidden`만 실행하면 보이지 않는 프로세스에 종료 조건이 없었다.
- pixel shader가 큐브까지 clear color로 출력해도 기존 스모크는 종료 코드 0만 보고 통과했다.
- 렌더 확인 전에 `WM_QUIT`가 들어오면 실제 픽셀을 읽지 않고도 종료 코드 0을 반환했다.
- 프레임 제한으로 끝낸 창이 남긴 `WM_QUIT` 때문에 같은 thread의 다음 창이 바로 종료됐다.
- WARP와 셰이더 테스트를 한 정규식으로 확인하니 둘 중 하나만 있어도 등록 검증이 통과했다.

HLSL `POST_BUILD` 복사가 stale 파일을 남긴다는 가설은 현재 Visual Studio 생성기에서 재현되지 않았다. HLSL만 바꾼 뒤 다시 빌드했을 때 배포본 SHA도 함께 바뀌었다. 이 항목은 구조를 바꾸는 대신 원본과 배포본 SHA-256을 비교하는 테스트로 감시한다.

## 관찰

리소스 풀은 free-list에서 slot을 먼저 빼고 나서 값을 생성했다. 생성자가 예외를 던지면 빈 slot이 목록으로 돌아오지 않았다. 새 slot에서도 빈 항목이 container 끝에 남았다.

입력은 현재 상태와 이전 프레임 상태만 비교했다. 한 프레임 안에서 두 전이가 모두 일어나 최종 상태가 원래대로 돌아오면 중간 edge를 복원할 정보가 없었다.

렌더 스모크는 장치와 셰이더 생성 실패는 잡았지만 `DrawIndexed` 결과는 보지 않았다. draw를 없애거나 clear color만 출력해도 프로세스는 정상 종료할 수 있었다.

창 종료에는 사용자가 닫은 경우와 `EngineApp`이 소유한 창을 정리하는 경우가 섞여 있었다. 두 경우 모두 `WM_DESTROY`에서 `PostQuitMessage`를 호출해, 정상적인 객체 정리까지 다음 실행을 종료시키는 thread 메시지를 남겼다.

테스트 등록 검증은 두 이름을 묶은 정규식을 사용했다. `--no-tests=error`는 정규식에 일치하는 테스트가 하나라도 있으면 성공하기 때문에 두 테스트가 모두 존재한다는 보장이 되지 않았다.

## 선택

- 리소스 값 생성이 성공한 뒤에만 free-list를 갱신하고 새 slot 실패는 즉시 rollback한다.
- 저장소는 `deque`로 바꿔 비이동 자원과 성장 후 안정 주소를 허용한다.
- pressed와 released를 프레임 동안 따로 누적하고 포커스 상실 시 held key를 모두 해제한다.
- 시스템 키는 기본 처리에 전달하고 Alt+F4는 `WM_CLOSE`를 명시적으로 보장한다.
- 숨김 실행과 pixel 검증에는 반드시 frame limit을 요구한다.
- 마지막 프레임의 back buffer를 읽어 clear color가 아닌 RGB 픽셀이 하나 이상 있어야 스모크가 통과한다.
- 요청한 프레임까지 pixel 확인을 마치지 못하면 정상 종료로 처리하지 않는다.
- 사용자가 창을 닫을 때만 thread 종료 메시지를 보내고 소유자 정리에서는 보내지 않는다.
- 포커스 상실은 `Window` 메시지 경계를 통과하는 테스트로 held key 해제를 확인한다.
- WARP와 셰이더 테스트 이름을 따로 조회해 어느 한쪽이 빠져도 등록 검증이 실패하게 한다.

## 검증

```text
Windows Debug 빌드: 성공
Windows Release 빌드: 성공
CTest: 27/27 통과
WARP named smoke: 1/1 통과
shader deployment named test: 1/1 통과
WARP 320x180 3프레임 non-clear pixel: 통과
RTX 하드웨어 960x540 120프레임 non-clear pixel: 종료 코드 0
```

pixel shader를 clear color 상수로 바꾼 RED에서는 `render verification found only the clear color`로 스모크가 실패했다. 시스템 키 기본 처리를 제거한 RED에서는 Alt+F4 테스트가 실패했다. 렌더 전 종료와 연속 창 실행을 추가한 RED에서는 각각 검증 없는 성공과 다음 창의 즉시 종료가 재현됐다. 묶인 스모크 정규식은 존재하지 않는 두 번째 이름을 넣어도 실제 테스트 하나만 실행한 뒤 종료 코드 0을 반환했다. 수정 후 전체 suite와 이름별 등록 검증을 다시 실행했다.

## 남은 한계

- debug layer의 warning과 live object를 아직 자동 판정하지 않는다.
- pixel readback은 큐브의 정확한 모양과 색상을 검증하지 않는다.
- feature level 11.1 요청이 거부되는 구형 runtime fallback은 현재 지원 대상에 넣지 않았다.

다음 에셋 단계에 들어가기 전에 이 상태를 두 번째 PR의 기준점으로 남긴다.
