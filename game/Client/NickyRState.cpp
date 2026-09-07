#include "pch.h"
#include "NickyRState.h"
#include "ModelAnimator.h"

NickyRState::NickyRState(shared_ptr<ModelAnimator> modelAnimator)
    :Super(PlayerStateType::Skill_4)
    , m_modelAnimator(modelAnimator)
{

}

NickyRState::~NickyRState()
{

}

void NickyRState::Enter()
{
    m_expectedDuration = 0.f;
    //재생속도에 따라 애니메이션 속도들 재설정
    m_sequenceDurations = m_modelAnimator->GetSequenceAnimationDurations(L"Skill_4_Sequence");
    
    for (size_t i = 0; i < m_sequenceDurations.size(); i++)
    {
        if (i == 2)
        {
            cout << "PlayerState 에서 추가되는 시간 : " << m_sequenceDurations[i] << endl;
            m_expectedDuration += (m_sequenceDurations[i]);
        }
        else
        {
            cout << "PlayerState 에서 추가되는 시간 : " << (m_sequenceDurations[i] / 2.f) << endl;
            m_expectedDuration += (m_sequenceDurations[i] / 2.f);
        }
    }

    cout << "PlayerState 에서의 기대 시간 : " << m_expectedDuration << endl;

    m_skillTime = 0.0f;
    m_isSkillComplete = false;
   
    cout << "NickyRState진입\n";
}

void NickyRState::Update()
{
    // 대기 시간 업데이트
    m_skillTime += DT;

    // 시퀀스 재생 상태 체크
    if (!m_isSkillComplete && m_skillTime >= m_expectedDuration)
    {
        // 시퀀스가 끝났으면 완료 플래그 설정
        m_isSkillComplete = true;
        cout << "R 스킬 시퀀스 자동 완료 감지" << endl;
    }
}

void NickyRState::Exit()
{
    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isSkillComplete = false;

    cout << "NickyRState종료\n";
}

bool NickyRState::CanTransitionTo(PlayerStateType newState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if (m_isSkillComplete && (newState == PlayerStateType::Wait || newState == PlayerStateType::Run))
    {
        return true;
    }
    return false;
}
