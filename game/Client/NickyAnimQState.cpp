#include "pch.h"
#include "NickyAnimQState.h"
#include "ModelAnimator.h"
#include "GameObject.h"
#include "Model.h"
#include "ModelAnimation.h"
#include "NavMeshAgent.h"
#include "Component.h"

NickyAnimQState::NickyAnimQState()
    : AnimationState(AnimationStateType::Skill_1)
{
}

void NickyAnimQState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;
    animator->SetAnimationSpeed(2.f);
    m_cachedAnimator = animator;

    // 기존 시퀀스나 애니메이션 중단
    if (animator->IsSequencePlaying())
    {
        animator->StopSequence();
    }

    // 초기 상태 설정
    m_chargeTime = 0.0f;
    m_skillTime = 0.0f;
    m_isReleasing = false;
    m_isComplete = false;
    m_isChargingActive = true;
    m_isStartAnimationPlaying = false;
    m_startAnimationTime = 0.0f;

    cout << "Nicky Q 스킬 차징 애니메이션 상태 진입" << endl;

    // NavMeshAgent 상태를 기반으로 초기 상태 설정
    SetInitialMovementState(false); // wasMoving 매개변수는 이제 사용하지 않음
}

void NickyAnimQState::Update(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    m_skillTime += DT;

    // 현재 재생 중인 애니메이션 확인 (디버깅용)
    static float debugTimer = 0.0f;
    debugTimer += DT;
    if (debugTimer >= 1.0f)  // 1초마다 출력
    {
        wstring currentAnim = animator->GetCurrentAnimationTag();
        /* cout << "현재 재생 중인 애니메이션: " << string(currentAnim.begin(), currentAnim.end()) << endl;
         cout << "시퀀스 모드: " << (animator->IsSequencePlaying() ? "ON" : "OFF") << endl;
         cout << "차징 상태: " << (int)m_chargeState << endl;
         cout << "차징 활성: " << (m_isChargingActive ? "YES" : "NO") << endl;*/
        debugTimer = 0.0f;
    }

    // 스킬 입력 처리 (Q 키 해제 감지)
    HandleSkillInput();

    // 이동 입력 처리 (차징 중일 때만)
    if (m_isChargingActive)
    {
        HandleMovementInput();
    }

    // 상태별 업데이트
    if (m_isReleasing)
    {
        UpdateReleasing();
    }
    else if (m_isChargingActive)
    {
        UpdateCharging();
    }
}

void NickyAnimQState::HandleSkillInput()
{
    // Q 키 해제 감지
    if (INPUT->GetButtonUp(KEY_TYPE::Q) && m_isChargingActive)
    {
        ReleaseSkill();
    }

    // ESC 키로 차징 취소 (선택사항)
    if (INPUT->GetButtonDown(KEY_TYPE::ESC) && m_isChargingActive)
    {
        cout << "Q 스킬 차징 취소" << endl;
        m_isComplete = true;  // 강제로 완료 상태로 전환하여 Wait로 돌아감
    }
}

void NickyAnimQState::HandleMovementInput()
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

        //cout << "차징 중 이동 상태 변화: " << (currentlyMoving ? "이동" : "정지") << endl;

        if (currentlyMoving)
        {
            // 정지 -> 이동
            if (m_chargeState == QSkillChargeState::ChargingWaitLoop)
            {
                TransitionToChargeState(QSkillChargeState::ChargingRunLoop);
            }
            else if (m_chargeState == QSkillChargeState::ChargingFromWait && !m_isStartAnimationPlaying)
            {
                // 시작 애니메이션이 끝났으면 런 루프로
                TransitionToChargeState(QSkillChargeState::ChargingRunLoop);
            }
        }
        else
        {
            // 이동 -> 정지
            if (m_chargeState == QSkillChargeState::ChargingRunLoop)
            {
                TransitionToChargeState(QSkillChargeState::ChargingWaitLoop);
            }
        }
    }
}

void NickyAnimQState::UpdateCharging()
{
    if (!m_cachedAnimator)
        return;

    m_chargeTime += DT;
    //cout << "사용된 DT : " << DT << endl;

    // 시작 애니메이션 재생 중인지 확인
    if (m_isStartAnimationPlaying)
    {
        m_startAnimationTime += DT;

        // 시작 애니메이션 완료 체크
        if (IsStartAnimationComplete())
        {
            m_isStartAnimationPlaying = false;

            // 루프 애니메이션으로 전환
            if (m_isMoving)
            {
                TransitionToChargeState(QSkillChargeState::ChargingRunLoop);
            }
            else
            {
                TransitionToChargeState(QSkillChargeState::ChargingWaitLoop);
            }
        }
    }

    // 최대 차징 시간 체크
    if (m_chargeTime >= m_maxChargeTime)
    {
        cout << "최대 차징 시간 도달 - 자동 발동" << endl;
        ReleaseSkill();
    }
}

void NickyAnimQState::UpdateReleasing()
{
    if (!m_cachedAnimator)
        return;

    // 현재 애니메이션 확인
    wstring currentAnim = m_cachedAnimator->GetCurrentAnimationTag();

    // Rush 애니메이션이 재생 중일 때만 true
    m_isFirstAnimationActive = (currentAnim == L"Skill_01_Rush");

    if (currentAnim == L"Skill_01_End")
    {
        // End 애니메이션이 완료되면 스킬 완료
        if (m_cachedAnimator->IsAnimationFinished())
        {
            m_isComplete = true;
            cout << "Nicky Q 스킬 애니메이션 완료!" << endl;
        }
    }
    else
    {
        if (currentAnim == L"Skill_01_Attack" || currentAnim == L"Skill_01_Rush") return;
        else
            m_isComplete = true;
    }

    //if (!m_cachedAnimator)
    //    return;

    //wstring currentAnim = m_cachedAnimator->GetCurrentAnimationTag();

    //// 강제 종료된 경우 또는 일반적인 End 애니메이션 처리
    //if (m_isForcedEnd || currentAnim == L"Skill_01_End")
    //{
    //    if (m_cachedAnimator->IsAnimationFinished())
    //    {
    //        m_isComplete = true;
    //        cout << "Nicky Q 스킬 애니메이션 완료!" << endl;
    //    }
    //}
    //else
    //{
    //    // Rush 애니메이션이 재생 중일 때만 true
    //    m_isFirstAnimationActive = (currentAnim == L"Skill_01_Rush");

    //    if (currentAnim == L"Skill_01_Attack" || currentAnim == L"Skill_01_Rush")
    //        return;
    //    else
    //        m_isComplete = true;
    //}
}

void NickyAnimQState::SetInitialMovementState(bool wasMoving)
{
    // NavMeshAgent의 현재 상태로 초기 이동 상태 설정
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
            m_isMoving = false; // NavMeshAgent가 없으면 정지 상태로 간주
        }
    }

    // cout << "초기 이동 상태 설정: " << (m_isMoving ? "이동" : "정지") << endl;

    if (m_isMoving)
    {
        // 달리는 상태에서 시작 - 바로 루프로
        TransitionToChargeState(QSkillChargeState::ChargingRunLoop);
    }
    else
    {
        // 멈춘 상태에서 시작 - 시작 애니메이션 재생
        TransitionToChargeState(QSkillChargeState::ChargingWaitLoop);
    }
}

void NickyAnimQState::ReleaseSkill()
{
    if (m_isReleasing)
        return;

    m_isReleasing = true;
    m_isChargingActive = false;

    cout << "Nicky Q 스킬 애니메이션 발동! 차징 시간: " << m_chargeTime << "초" << endl;

    if (m_cachedAnimator)
    {
        // 차징 시간에 따른 Rush 시퀀스 생성
        vector<wstring> rushSequence = { L"Skill_01_Rush", L"Skill_01_End" };
        vector<float> rushDurations = { min(m_chargeTime, 5.0f), (13.f / 25.f) / m_playSpeed }; // 차징 시간만큼 재생

        // 동적 시퀀스 생성
        m_cachedAnimator->CreateSequence(L"Dynamic_Rush_Sequence", rushSequence, rushDurations, false);

        // 시퀀스 재생
        m_cachedAnimator->PlaySequence(L"Dynamic_Rush_Sequence");
    }
}

shared_ptr<GameObject> NickyAnimQState::GetGameObject() const
{
    if (m_cachedAnimator)
    {
        return m_cachedAnimator->GetGameObject();
    }
    return nullptr;
}

void NickyAnimQState::TransitionToChargeState(QSkillChargeState newState)
{
    if (m_chargeState == newState)
        return;

    QSkillChargeState oldState = m_chargeState;
    m_chargeState = newState;

    //cout << "차징 상태 전환: " << (int)oldState << " -> " << (int)newState << endl;

    PlayAppropriateAnimation();
}

void NickyAnimQState::PlayAppropriateAnimation()
{
    if (!m_cachedAnimator)
        return;

    wstring animationTag;

    switch (m_chargeState)
    {
    case QSkillChargeState::ChargingFromWait:
        animationTag = L"Skill_01_Charge_Start_Wait";
        m_isStartAnimationPlaying = true;
        m_startAnimationTime = 0.0f;
        //cout << "차징 시작 (Wait)" << endl;
        break;

    case QSkillChargeState::ChargingFromRun:
        animationTag = L"Skill_01_Charge_Start_Run";
        m_isStartAnimationPlaying = true;
        m_startAnimationTime = 0.0f;
        //cout << "차징 시작 (Run)" << endl;
        break;

    case QSkillChargeState::ChargingWaitLoop:
        animationTag = L"Skill_01_Charge_Loop_Wait";
        //cout << "차징 루프 (Wait)" << endl;
        break;

    case QSkillChargeState::ChargingRunLoop:
        animationTag = L"Skill_01_Charge_Loop_Run";
        //cout << "차징 루프 (Run)" << endl;
        break;

    default:
        //cout << "알 수 없는 차징 상태: " << (int)m_chargeState << endl;
        return;
    }

    //cout << "애니메이션 재생 시도: " << string(animationTag.begin(), animationTag.end()) << endl;

    // 강제로 즉시 애니메이션 재생 (블렌딩 없이)
    m_cachedAnimator->SetAnimationByTag(animationTag, true);

    // 애니메이션 재생 후 확인
    wstring currentAnim = m_cachedAnimator->GetCurrentAnimationTag();
    //cout << "실제 재생된 애니메이션: " << string(currentAnim.begin(), currentAnim.end()) << endl;
}

bool NickyAnimQState::IsStartAnimationComplete()
{
    if (!m_cachedAnimator)
        return true;

    // 실제 애니메이션 길이 기반 계산
    wstring currentAnim = m_cachedAnimator->GetCurrentAnimationTag();
    float animDuration = m_cachedAnimator->GetAnimationDuration(currentAnim);

    // 애니메이션 길이의 90%가 지나면 완료로 간주
    return m_startAnimationTime >= (animDuration * 1.0f);
}

void NickyAnimQState::Exit(shared_ptr<ModelAnimator> animator)
{
    //if (!animator)
    //    return;

    //cout << "Nicky Q 스킬 애니메이션 종료 " << endl;

    //// 상태 정리
    //m_chargeTime = 0.0f;
    //m_skillTime = 0.0f;
    //m_isReleasing = false;

    //m_isComplete = false;
    //m_isChargingActive = false;
    //m_isStartAnimationPlaying = false;
    //m_cachedAnimator.reset();

    //animator->SetAnimationSpeed(1.f);

    //m_chargeState = QSkillChargeState::Default;

    if (!animator)
        return;

    cout << "Nicky Q 스킬 애니메이션 종료" << endl;

    // 상태 정리
    m_chargeTime = 0.0f;
    m_skillTime = 0.0f;
    m_isReleasing = false;
    m_isComplete = false;
    m_isChargingActive = false;
    m_isStartAnimationPlaying = false;
    m_isForcedEnd = false;  // 강제 종료 플래그도 리셋
    m_cachedAnimator.reset();

    animator->SetAnimationSpeed(1.f);
    m_chargeState = QSkillChargeState::Default;
}

bool NickyAnimQState::CanTransitionTo(AnimationStateType nextState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    return (m_isComplete && (nextState == AnimationStateType::Wait || nextState == AnimationStateType::Run));
}

bool NickyAnimQState::IsCharging() const
{
    return m_isChargingActive;
}

bool NickyAnimQState::IsComplete() const
{
    return m_isComplete;
}

void NickyAnimQState::ForceEndAnimation()
{
    if (!m_cachedAnimator) return;

    cout << "Q 스킬 애니메이션 강제 종료!" << endl;

    m_isForcedEnd = true;
    m_isReleasing = true;
    m_isChargingActive = false;

    // End 애니메이션으로 즉시 변경
    vector<wstring> endSequence = { L"Skill_01_End" };
    vector<float> endDurations = { (13.f / 25.f) / m_playSpeed };

    m_cachedAnimator->StopSequence();

    m_cachedAnimator->CreateSequence(L"Force_End_Sequence", endSequence, endDurations, false);
    m_cachedAnimator->PlaySequence(L"Force_End_Sequence");
    m_cachedAnimator->SetCurrentAnimationSpeed(m_playSpeed);
}