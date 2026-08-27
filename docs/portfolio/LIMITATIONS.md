# 포트폴리오 통합 한계

아래 경계는 다섯 사례의 성공 조건을 현재 제품 전체의 검증으로 확대하지 않기 위해 함께 공개한다. 개별 수치의 실행 조건과 원본 상태는 [근거 매트릭스](EVIDENCE_MATRIX.md)에서 확인한다.

## Graphics

network 부하의 자동 그래픽 경로는 Microsoft Basic Render Driver WARP였다. RTX 3050 Ti에서 얻은 렌더 benchmark와 WARP network 결과는 서로 섞지 않는다. 렌더 장면도 한 종류 prototype mesh 중심의 synthetic scene이어서 실제 맵의 material 다양성, overdraw와 긴 플레이를 대표하지 않는다. 다음 그래픽 검증은 실제 게임 화면의 RTX visible play와 장시간 실행을 별도 원본으로 남겨야 한다.

## Simulation

NavMesh와 오프라인 경기는 높이와 장애물이 없는 평면 맵을 사용했다. 동적 NavMesh, actor body blocking, projectile flight와 quadtree reinsertion 비용은 검증하지 않았다. 600초 timeout의 순위 판정은 개발용 종료 보장이며 무승부나 reconnect 뒤 경기 복원을 표현하지 않는다.

## Network

game TCP와 UDP transport는 plaintext이며 reconnect와 endpoint rebind가 없다. application shaper는 고정 seed의 delay, jitter와 loss 복구를 재현하지만 kernel queue, NAT와 route change를 대신하지 않는다. worker control에는 mutual authentication이 없어 외부 네트워크에 그대로 노출할 경계가 아니다. transport 보호, worker 신원 확인과 reconnect protocol을 결정하기 전에는 인터넷 서비스 보안 검증으로 표시하지 않는다.

## Deployment

11주차는 local packaging까지만 수행했고 AWS resource와 외부 접속은 실행하지 않았다. 따라서 cloud 성능, 보안 그룹, 운영 비용, 종료와 잔여 비용은 모두 미검증 상태다. 계정, 결제와 외부 승인 뒤 실제 배포를 선택한다면 생성 전 조건과 종료 확인을 새 실행 기록으로 남겨야 한다.

## Evidence

채택 수치는 코드 기준 SHA `884e5e70d68d9fcf9dfe5638d97e06623da154c2`에서 추적하지만 각 역사적 benchmark의 실행 commit은 서로 다르다. 특히 Windows network 성능 evidence는 `01ae1278`, Linux sanitizer evidence는 `442f115` 기준이어서 같은 SHA의 교차 플랫폼 결과가 아니다. 검증기는 경로와 원문 수치 문자열을 확인할 뿐 수치를 재계산하거나 현재 HEAD 성능을 증명하지 않는다.

## Product scope

현재 범위는 경기 맵 한 종과 플레이 가능한 character 한 종을 중심으로 한 기술 수직 기능이다. matchmaking 운영, 계정과 persistence, reconnect, 다양한 맵과 character 조합은 제외됐다. PDF, 실제 데모 영상, 저장소 공개와 `v0.1.0` 태그도 [기반 설계](../superpowers/specs/2026-08-27-portfolio-foundation-design.md)의 다음 계획이며 이번 문서 작업만으로 완료 처리하지 않는다.
