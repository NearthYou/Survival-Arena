#include "pch.h"
#include "WolfAttack1State.h"

WolfAttack1State::WolfAttack1State()
	: AnimationState(AnimationStateType::BaseAttack)
{
}

void WolfAttack1State::Enter(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;
    _animator->SetAnimationSpeed(m_playSpeed);
    // Wait 애니메이션 재생
    _animator->SetAnimationByTag(L"Atk1", false);

    m_attackTime = 0.0f;
    m_isAnimationStarted = true;

    cout << "Wait 상태 진입 - Wait 애니메이션 재생 시작" << endl;
}

void WolfAttack1State::Update(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    // 대기 시간 업데이트
    m_attackTime += DT;

    // 애니메이션이 정상적으로 재생되고 있는지 확인
    if (m_isAnimationStarted)
    {
        wstring currentAnimTag = _animator->GetCurrentAnimationTag();
        if (currentAnimTag == L"Atk1")
        {
            // Wait 애니메이션이 정상적으로 재생 중
            // 필요시 추가 로직 구현
        }
    }
}

void WolfAttack1State::Exit(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    cout << "Wait 상태 종료 - 대기 시간: " << m_attackTime << "초" << endl;

    // 상태 종료 시 정리
    m_attackTime = 0.0f;
    m_isAnimationStarted = false;

    _animator->SetAnimationSpeed(1.f);
}

bool WolfAttack1State::CanTransitionTo(AnimationStateType _nextState)
{
    // Wait 상태에서는 대부분의 상태로 전환 가능
    switch (_nextState)
    {
    case AnimationStateType::Move:
    case AnimationStateType::Run:
    case AnimationStateType::Wait:
        return true;
    case AnimationStateType::BaseAttack:
        return true;  // 자기 자신으로도 이건 전환 가능. 
    default:
        return false;
    }
}
