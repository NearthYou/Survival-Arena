#include "pch.h"
#include "AlphaAttackAI.h"
#include "Player.h"
#include "Monster.h"

AlphaAttackAI::AlphaAttackAI(shared_ptr<Monster> _Owner)
	: Super(_Owner)
{
}

AlphaAttackAI::~AlphaAttackAI()
{
}

void AlphaAttackAI::Enter()
{
	m_Owner->GetAnimationStateMachine()->ChangeState(AnimationStateType::Move);
	m_enterPos = m_Owner->GetTransform()->GetPosition();
	m_Owner->SetMonsterState(MonsterState::RUN);

	m_Owner->GetNavMeshAgent()->SetSpeed(m_Owner->GetMonsterStatus().moveSpeed);
	m_Owner->GetNavMeshAgent()->SetDestination(m_Owner->GetTarget()->GetTransform()->GetPosition());
	wcout << L"Alpha Enter Attack AI\n";
}

void AlphaAttackAI::Update()
{
	auto target = m_Owner->GetTarget();
	auto& monsterStatus = m_Owner->GetMonsterStatus();
	skillCoolTime -= DT;

	//Target이 없으면 return;
	if (target == nullptr)
		return;

	//달려가고 있을 때, 플레이어가 멀리 가버리면 원래 자리로 돌아감. 
	if (returnEnterPos && m_Owner->GetMonsterState() == MonsterState::RUN) {
		return;
	}

	if (m_Owner->GetMonsterState() == MonsterState::ATTACK) {
		attackElapsedTime += DT;

		if (attackElapsedTime >= (1.0f / monsterStatus.hitSpeed)) {
			//데미지 주기. 

			m_Owner->GetAnimationStateMachine()->ChangeState(AnimationStateType::Move);
			m_Owner->SetMonsterState(MonsterState::RUN);
			attackElapsedTime = 0.f;
		}
		return;
	}

	if (m_Owner->GetMonsterState() == MonsterState::SKILL) {
		skillElapsedTime += DT;
		
		if (skillElapsedTime >= skillDuration) {
			//데미지 주기.

			m_Owner->GetAnimationStateMachine()->ChangeState(AnimationStateType::Move);
			m_Owner->SetMonsterState(MonsterState::RUN);
			skillElapsedTime = 0.f;
		}

		return;
	}

	//target과 자신의 거리 재기. 
	Vec3 ownerPosition = m_Owner->GetTransform()->GetPosition();
	Vec3 targetPosition = target->GetTransform()->GetPosition();

	float distance = Vec3::Distance(ownerPosition, targetPosition);

	//거리가 일정 수준 바깥으로 나가면 돌아가기 Anim.
	if (distance > m_RecogRange) {
		returnEnterPos = true;
		m_Owner->GetNavMeshAgent()->SetSpeed(3.12f);
		m_Owner->GetNavMeshAgent()->SetDestination(m_enterPos);
	}
	//만약 플레이어와의 거리가 공격 Range보다 작거나 같으면.
	//공격 사거리 안에 들어왔을 경우. 
	else if (distance <= m_SkillRange && skillCoolTime < 0.f) {
		m_Owner->GetAnimationStateMachine()->ChangeState(AnimationStateType::Skill_1);
		m_Owner->SetMonsterState(MonsterState::SKILL);
		m_Owner->GetNavMeshAgent()->Stop();
		skillCoolTime = 15.f;
	}
	else if (distance <= monsterStatus.hitRange) {
		m_Owner->GetAnimationStateMachine()->ChangeState(AnimationStateType::BaseAttack);
		m_Owner->SetMonsterState(MonsterState::ATTACK);
		m_Owner->GetNavMeshAgent()->Stop();
	}
}

void AlphaAttackAI::Exit()
{
	m_Owner->SetMoveSpeed(1.56f);
	returnEnterPos = false;
	m_enterPos = Vec3::Zero;
	attackElapsedTime = 0.f;
	m_target.reset();
}
