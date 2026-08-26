# AWS 비용 및 보안 확인표

- 상태: 실행 전 확인 필요
- 확인 기준일: 2026-08-26
- 기본 후보 region: `ap-northeast-2`
- 기본 후보 instance: `t3.small`

이 문서는 AWS resource를 만드는 script가 아니다. 계정, 비용과 접속 원본 IP를 확인하기 전에는 instance, security group, Elastic IP와 volume을 만들지 않는다.

## 후보를 고른 근거

AWS 공식 region 표에는 서울 리전에서 T3 family를 제공한다고 나와 있다.

- https://docs.aws.amazon.com/ec2/latest/instancetypes/ec2-instance-regions.html

AWS T3 사양 표에서 `t3.small`은 2 vCPU, 2GiB memory, vCPU당 baseline 20%와 시간당 CPU credit 24를 가진다. 이 페이지의 표시 가격은 미국 동부 기준이므로 서울 비용으로 사용하지 않는다.

- https://aws.amazon.com/ec2/instance-types/t3/

10주차 30분 impairment soak에서 game server tick P95는 1.8424ms였다. Windows working set 232개 sample의 game server P95는 48.61MiB, 최대는 49.52MiB였고 lobby server P95는 27.66MiB였다. 원본은 다음 경로에 있다.

- `docs/benchmarks/network-load/20260826-050345-01ae1278-interest-delta-impairment-soak30/summary.json`
- 같은 run의 `match-001`부터 `match-004`까지의 `working-set.csv`

이 수치는 `t3.small`이 첫 측정 후보라는 근거일 뿐이다. Windows working set을 Linux RSS로 바꾸어 표현하지 않고, burstable CPU credit이 두 경기 동시 부하를 감당한다고 미리 결론내리지 않는다.

## 계정과 제공 여부 확인

다음 명령은 resource를 만들지 않는 조회다. 실제 실행 전 선택한 AWS profile과 account를 사용자에게 다시 보여준다.

```powershell
aws sts get-caller-identity

aws ec2 describe-instance-type-offerings `
  --region ap-northeast-2 `
  --location-type region `
  --filters Name=instance-type,Values=t3.small

aws ec2 describe-instance-types `
  --region ap-northeast-2 `
  --instance-types t3.small
```

`DescribeInstanceTypeOfferings`의 의미는 AWS API 문서에서 확인한다.

- https://docs.aws.amazon.com/AWSEC2/latest/APIReference/API_DescribeInstanceTypeOfferings.html

## 비용 확인

서울 Linux On-Demand 가격은 resource 생성 직전에 AWS Pricing Calculator 또는 Price List API로 다시 조회한다. AWS는 Price List API 값을 정보 제공용으로 설명하며 서비스 가격 페이지와 차이가 있으면 서비스 가격을 적용한다고 밝힌다.

- https://docs.aws.amazon.com/awsaccountbilling/latest/aboutv2/price-changes.html
- https://docs.aws.amazon.com/cli/latest/reference/pricing/get-products.html

확인할 비용은 다음과 같다.

1. `t3.small` Linux On-Demand 실행 시간
2. T3 Unlimited에서 baseline을 넘긴 CPU 사용 비용
3. EBS root volume 유형, 크기와 snapshot
4. public IPv4 주소 사용 시간
5. 인터넷 데이터 전송량
6. 세금과 환율

AWS 문서는 public IPv4 주소에 시간당 0.005 USD가 부과된다고 설명한다. 730시간을 계속 사용하면 IPv4 항목만 3.65 USD이지만, instance와 EBS 및 전송 비용은 별도다.

- https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/using-instance-addressing.html

다음 값을 채우기 전에는 생성 단계로 넘어가지 않는다.

```text
AWS profile:
확인한 account:
region:
instance type:
AMI와 architecture:
EBS 유형과 크기:
public IPv4 또는 DNS 방식:
예상 실행 시간:
가격 확인 시각과 출처:
세전 예상 합계:
종료 예정 시각:
```

## security group 계획

| port | protocol | source | 목적 |
| --- | --- | --- | --- |
| 22 | TCP | 사용자 공인 IPv4 `/32` | SSH |
| 7000 | TCP | demo 참가자 범위 | lobby |
| 7100 | TCP | demo 참가자 범위 | worker 1 인증 |
| 7101 | UDP | demo 참가자 범위 | worker 1 input과 snapshot |
| 7200 | TCP | demo 참가자 범위 | worker 2 인증 |
| 7201 | UDP | demo 참가자 범위 | worker 2 input과 snapshot |

7001/TCP worker control rule은 만들지 않는다. AWS는 SSH 22번을 필요한 특정 IP 또는 범위로만 제한할 것을 권장한다.

- https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/changing-security-group.html

게임 port도 가능하면 실제 demo 참가자 CIDR로 제한한다. 공개 demo 때문에 전체 IPv4가 필요하면 종료 시각을 먼저 정하고 테스트 직후 rule과 instance를 제거한다.

## 종료 확인

cloud 검증이 끝나면 다음 항목을 확인한다.

1. Compose project 종료
2. EC2 instance 중지 또는 종료
3. 불필요한 EBS volume과 snapshot 제거
4. Elastic IP를 사용했다면 연결 해제와 release 확인
5. 임시 security group 제거
6. Cost Explorer 또는 Billing 화면에서 잔여 예상 비용 확인

삭제 대상의 ID와 account 및 region을 다시 확인하기 전에는 제거 명령을 실행하지 않는다.
