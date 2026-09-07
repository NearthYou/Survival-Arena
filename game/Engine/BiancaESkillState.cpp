#include "pch.h"
#include "BiancaESkillState.h"
#include "ModelAnimator.h"
#include "GameObject.h"
#include "Model.h"
#include "ModelAnimation.h"
#include "NavMeshAgent.h"
#include "Component.h"

BiancaESkillState::BiancaESkillState()
    : AnimationState(AnimationStateType::Skill_3)
{
}

void BiancaESkillState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;
    animator->SetAnimationSpeed(m_playSpeed);
    m_cachedAnimator = animator;

    // 기존 시퀀스나 애니메이션 중단
    if (animator->IsSequencePlaying())
    {
        animator->StopSequence();
    }

    // 초기 상태 설정
    m_chargeTime = 0.0f;
    m_skillTime = 0.0f;
    m_isCharging = true;
    m_isReleasing = false;
    m_isEnding = false;
    m_isComplete = false;

    cout << "비앙카 E 스킬 상태 진입" << endl;

    // NavMeshAgent 상태를 기반으로 초기 이동 상태 설정
    auto gameObject = GetGameObject();
    if (gameObject)
    {
        auto navMeshAgent = gameObject->GetFixedComponent<NavMeshAgent>(ComponentType::NavMeshAgent);
        if (navMeshAgent)
        {
            m_isMoving = navMeshAgent->IsMoving();
        }
        else
        {
            m_isMoving = false;
        }
    }

    // 초기 차징 상태 설정
    if (m_isMoving)
    {
        TransitionToSkillState(BiancaESkillChargeState::ChargingRun);
    }
    else
    {
        TransitionToSkillState(BiancaESkillChargeState::ChargingWait);
    }
}

void BiancaESkillState::Update(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    m_skillTime += DT;

    // 디버깅 정보 출력
    static float debugTimer = 0.0f;
    debugTimer += DT;
    if (debugTimer >= 1.0f)  // 1초마다 출력
    {
        wstring currentAnim = animator->GetCurrentAnimationTag();
        cout << "비앙카 E 스킬 - 현재 애니메이션: " << string(currentAnim.begin(), currentAnim.end()) << endl;
        cout << "스킬 상태: " << (int)m_skillState << endl;
        cout << "차징 시간: " << m_chargeTime << "초" << endl;
        cout << "이동 상태: " << (m_isMoving ? "이동" : "정지") << endl;
        debugTimer = 0.0f;
    }

    // 스킬 입력 처리 (E 키 해제 감지)
    HandleSkillInput();

    // 이동 입력 처리 (차징 중일 때만)
    if (m_isCharging)
    {
        HandleMovementInput();
    }

    // 상태별 업데이트
    switch (m_skillState)
    {
    case BiancaESkillChargeState::ChargingWait:
    case BiancaESkillChargeState::ChargingRun:
        UpdateCharging();
        break;
    case BiancaESkillChargeState::Releasing:
        UpdateReleasing();
        break;
    case BiancaESkillChargeState::Ending:
        UpdateEnding();
        break;
    }
}

void BiancaESkillState::HandleSkillInput()
{
    // E 키 해제 감지
    if (INPUT->GetButtonUp(KEY_TYPE::E) && m_isCharging)
    {
        ReleaseSkill();
    }

    // ESC 키로 스킬 취소 (선택사항)
    if (INPUT->GetButtonDown(KEY_TYPE::ESC) && m_isCharging)
    {
        cout << "비앙카 E 스킬 취소" << endl;
        m_isComplete = true;  // 강제로 완료 상태로 전환하여 Wait로 돌아감
    }
}

void BiancaESkillState::HandleMovementInput()
{
    // NavMeshAgent의 이동 상태 확인
    auto gameObject = GetGameObject();
    if (!gameObject) return;

    auto navMeshAgent = gameObject->GetFixedComponent<NavMeshAgent>(ComponentType::NavMeshAgent);
    if (!navMeshAgent) return;

    // NavMeshAgent의 이동 상태로 현재 이동 여부 판단
    bool currentlyMoving = navMeshAgent->IsMoving();

    // 이동 상태 변화 처리
    if (currentlyMoving != m_isMoving)
    {
        m_isMoving = currentlyMoving;

        cout << "차징 중 이동 상태 변화: " << (currentlyMoving ? "이동" : "정지") << endl;

        if (currentlyMoving)
        {
            // 정지 -> 이동
            if (m_skillState == BiancaESkillChargeState::ChargingWait)
            {
                TransitionToSkillState(BiancaESkillChargeState::ChargingRun);
            }
        }
        else
        {
            // 이동 -> 정지
            if (m_skillState == BiancaESkillChargeState::ChargingRun)
            {
                TransitionToSkillState(BiancaESkillChargeState::ChargingWait);
            }
        }
    }
}

void BiancaESkillState::UpdateCharging()
{
    if (!m_cachedAnimator)
        return;

    m_chargeTime += DT;

    // 최대 차징 시간 체크
    if (m_chargeTime >= m_maxChargeTime)
    {
        cout << "최대 차징 시간 도달 - 자동 발동" << endl;
        ReleaseSkill();
    }
}

void BiancaESkillState::UpdateReleasing()
{
    if (!m_cachedAnimator)
        return;

    // 시퀀스가 완료되면 스킬 완료 처리
    if (!m_cachedAnimator->IsSequencePlaying())
    {
        m_isComplete = true;
        cout << "비앙카 E 스킬 완료!" << endl;
    }
}

void BiancaESkillState::UpdateEnding()
{
    if (!m_cachedAnimator)
        return;

    // Skill_3_3이 완료되면 스킬 완료
    if (m_cachedAnimator->IsAnimationFinished())
    {
        m_isComplete = true;
        cout << "비앙카 E 스킬 완료!" << endl;
    }
}

shared_ptr<GameObject> BiancaESkillState::GetGameObject() const
{
    if (m_cachedAnimator)
    {
        return m_cachedAnimator->GetGameObject();
    }
    return nullptr;
}

void BiancaESkillState::ReleaseSkill()
{
    if (m_isReleasing)
        return;

    m_isCharging = false;
    m_isReleasing = true;

    cout << "비앙카 E 스킬 발동! 차징 시간: " << m_chargeTime << "초" << endl;

    if (m_cachedAnimator)
    {
        // 차징 시간에 따른 Skill_3_2 재생 시간 조절
        vector<wstring> releaseSequence = { L"Skill_3_2", L"Skill_3_3" };
        vector<float> releaseDurations = { min(m_chargeTime, 5.0f), (17.f / 25.f) / m_playSpeed }; // Skill_3_2는 차징 시간, Skill_3_3은 기본 시간

        // 동적 시퀀스 생성
        m_cachedAnimator->CreateSequence(L"Bianca_E_Release_Sequence", releaseSequence, releaseDurations, false);

        // 시퀀스 완료 콜백 설정
        m_cachedAnimator->SetSequenceCompleteCallback(L"Bianca_E_Release_Sequence", [this]() {
            m_isComplete = true;
            cout << "비앙카 E 스킬 시퀀스 완료!" << endl;
            });

        // 시퀀스 재생
        m_cachedAnimator->PlaySequence(L"Bianca_E_Release_Sequence");

        // 상태 변경
        TransitionToSkillState(BiancaESkillChargeState::Releasing);
    }
}

void BiancaESkillState::TransitionToSkillState(BiancaESkillChargeState newState)
{
    if (m_skillState == newState)
        return;

    BiancaESkillChargeState oldState = m_skillState;
    m_skillState = newState;

    cout << "비앙카 E 스킬 상태 전환: " << (int)oldState << " -> " << (int)newState << endl;

    switch (newState)
    {
    case BiancaESkillChargeState::ChargingWait:
        cout << "차징 중 (정지) - Wait 애니메이션 재생" << endl;
        m_cachedAnimator->SetAnimationByTag(L"Wait", true);
        break;

    case BiancaESkillChargeState::ChargingRun:
        cout << "차징 중 (이동) - Run 애니메이션 재생" << endl;
        m_cachedAnimator->SetAnimationByTag(L"Run", true);
        break;

    case BiancaESkillChargeState::Releasing:
        cout << "스킬 발동 - Skill_3_2 재생" << endl;
        // 시퀀스에서 처리되므로 여기서는 별도 처리 불필요
        break;

    case BiancaESkillChargeState::Ending:
        cout << "스킬 마무리 - Skill_3_3 재생" << endl;
        m_cachedAnimator->SetAnimationByTag(L"Skill_3_3", true);
        m_isReleasing = false;
        m_isEnding = true;
        break;

    case BiancaESkillChargeState::Complete:
        cout << "스킬 완료" << endl;
        m_isComplete = true;
        break;
    }
}

void BiancaESkillState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    cout << "비앙카 E 스킬 상태 종료 - 총 차징 시간: " << m_chargeTime << "초" << endl;

    // 상태 정리
    m_chargeTime = 0.0f;
    m_skillTime = 0.0f;
    m_isMoving = false;
    m_isCharging = false;
    m_isReleasing = false;
    m_isEnding = false;
    m_isComplete = false;
    m_skillState = BiancaESkillChargeState::ChargingWait;
    m_cachedAnimator.reset();

    animator->SetAnimationSpeed(1.f);
}

bool BiancaESkillState::CanTransitionTo(AnimationStateType nextState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    return m_isComplete && nextState == AnimationStateType::Wait;
}

bool BiancaESkillState::IsCharging() const
{
    return m_isCharging;
}

bool BiancaESkillState::IsComplete() const
{
    return m_isComplete;
}
