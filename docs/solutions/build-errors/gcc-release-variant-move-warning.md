---
title: GCC Release variant 이동의 미초기화 오탐 제거
date: 2026-08-26
category: build-errors
module: lobby server
problem_type: build_error
component: service_layer
symptoms:
  - "Ubuntu 24.04 GCC 13 Release build가 LobbyService.cpp의 maybe-uninitialized 경고를 Werror로 처리해 중단됨"
  - "같은 코드의 Linux Debug와 Windows build는 통과함"
root_cause: compiler_false_positive
resolution_type: code_fix
severity: medium
tags: [gcc, release-build, std-variant, emplace, werror]
---

# GCC Release variant 이동의 미초기화 오탐 제거

## Problem

Ubuntu 24.04 GCC 13의 Release `-O3 -Werror` build에서 `LobbyService.cpp`가 `std::variant`의 비활성 대안을 미초기화 값으로 진단했다. 배포 image가 Debug build로만 우회되지 않도록 Release 경계를 그대로 통과시켜야 했다.

## Symptoms

- `LobbyService.cpp`의 outbound message 추가 지점에서 `-Werror=maybe-uninitialized` 발생
- warning stack이 `std::variant` move constructor와 `std::vector::push_back` 내부로 이어짐
- protocol ID와 ticket field에는 이미 기본값이 있었음

## What Didn't Work

- 배포 build를 Debug로 낮추면 최적화된 server binary를 검증하지 못한다.
- `dxa_lobby_core` 전체에서 `-Wmaybe-uninitialized`를 끄면 이후 실제 미초기화 경고도 숨긴다.

## Solution

`OutboundMessage`가 `ServerMessage`로 만들 수 있는 message만 받아 직접 variant를 생성하게 했다.

```cpp
template <typename Message>
    requires std::constructible_from<
        dxa::protocol::ServerMessage,
        Message&&>
OutboundMessage(ConnectionId target, Message&& value)
    : recipient{target},
      message{std::forward<Message>(value)}
{
}
```

경고가 발생한 ticket과 worker 실패 응답은 aggregate 임시 객체를 `push_back`하지 않고 vector storage에 바로 만들었다.

```cpp
result.outbound.emplace_back(
    connection->second,
    dxa::protocol::MatchTicket{/* validated fields */});
```

구현은 `apps/lobby_server/include/dxa/lobby/LobbyService.hpp`와 `apps/lobby_server/src/LobbyService.cpp`에 있다.

## Why This Works

기존 경로는 `ServerMessage`를 가진 aggregate 임시 객체를 만든 뒤 vector로 옮겼다. GCC 13은 `-O3`에서 이 이동을 inline하며 선택되지 않은 variant 대안의 storage까지 추적해 오탐을 냈다. `emplace_back`은 선택된 message에서 최종 `OutboundMessage`를 vector storage에 직접 생성하므로 문제의 aggregate 이동 경로가 사라진다.

## Prevention

- Linux 배포 target은 Debug 검증과 별도로 Release `-Werror` build를 유지한다.
- compiler warning이 표준 라이브러리 내부에서 시작해도 먼저 caller의 임시 객체와 이동 경계를 줄이고, target 전체 warning suppression은 마지막 수단으로 남긴다.

## Related Issues

- [11주차 Linux 서버 패키징 기록](../../devlog/2026-08-26-linux-server-packaging.md)
