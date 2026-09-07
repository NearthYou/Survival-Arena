#include "pch.h"
#include "AlphaAnimSkillState.h"

AlphaAnimSkillState::AlphaAnimSkillState()
	: AnimationState(AnimationStateType::Skill_1)
{
	//Alpha_Skill_Sequence
}

AlphaAnimSkillState::~AlphaAnimSkillState()
{
}

void AlphaAnimSkillState::Enter(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    // Run 애니메이션 재생
    _animator->SetAnimationByTag(L"Skill2", false);  // 부드러운 전환

    m_animTime = 0.0f;
    m_isAnimationStarted = true;

    //cout << "Alpha Skill 상태 진입 - Skill 애니메이션 재생 시작" << endl;
}

void AlphaAnimSkillState::Update(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    // 대기 시간 업데이트
    //m_animTime += DT;

    if (m_isAnimationStarted)
    {
        // 스킬이 완료되면 자동으로 Wait 상태로 전환 요청
        // 실제 전환은 AnimationStateMachine에서 처리
        return;
    }

    // 시퀀스 재생 상태 체크
    if (m_isAnimationStarted && !_animator->IsSequencePlaying())
    {
        // 시퀀스가 끝났으면 완료 플래그 설정
       // m_isAppearComplete = true;
       // cout << "알파 Skill 애니메이션 완료." << endl;
    }
}

void AlphaAnimSkillState::Exit(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    //cout << "알파 Skill 상태 종료 " << endl;

    // 상태 종료 시 정리
    m_animTime = 0.0f;
    m_isAnimationStarted = false;
    _animator->SetAnimationSpeed(1.f);
}

bool AlphaAnimSkillState::CanTransitionTo(AnimationStateType _nextState)
{
    // Wait 상태에서는 대부분의 상태로 전환 가능
    switch (_nextState)
    {

    case AnimationStateType::BaseAttack:
    case AnimationStateType::Run:
    case AnimationStateType::Dying:
    case AnimationStateType::Death:
        return true;
    case AnimationStateType::Skill_1:
        return false;  // 자기 자신으로는 전환 불가
    default:
        return false;
    }
}
