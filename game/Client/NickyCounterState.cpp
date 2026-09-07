#include "pch.h"
#include "NickyCounterState.h"

#include "NickyCounter.h"

#include "Player.h"
#include "NickyWSkill.h"

NickyCounterState::NickyCounterState(shared_ptr<ModelAnimator> modelAnimator, shared_ptr<GameObject> _player)
    :Super(PlayerStateType::Counter)
    , m_modelAnimator(modelAnimator)
    , m_player(_player)
{

}

NickyCounterState::~NickyCounterState()
{

}

void NickyCounterState::Enter()
{
    cout << "니키 카운터 State진입!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
    auto player = static_pointer_cast<Player>(m_player);
    if (auto wSkill = static_cast<NickyWSkill*>(player->GetSkill(1)))  // W는 인덱스 1
    {
        wSkill->SkillEnd();  // 쿨타임 설정
        cout << "Counter 진입으로 W 스킬 쿨타임 설정" << endl;
    }
    // NickyBaseAttack 컴포넌트 활성화
    auto counterScript = m_player->GetComponent<NickyCounter>();
    if (counterScript) {
        counterScript->SetTarget(m_target);
        counterScript->StartCounter();
    }
    m_skillTime = 0.f;
    m_isSkillComplete = false; 
}
void NickyCounterState::Update()
{
    // 대기 시간 업데이트
    m_skillTime += DT;

    // 시퀀스 재생 상태 체크
    if (!m_isSkillComplete && m_skillTime >= (18.5/25.f) / 2.f)
    {
        // 시퀀스가 끝났으면 완료 플래그 설정
        m_isSkillComplete = true;
        cout << "Counter 스킬 시퀀스 자동 완료 감지" << endl;
    }
}
void NickyCounterState::Exit()
{
    // NickyBaseAttack 컴포넌트 활성화
    auto counterScript = m_player->GetComponent<NickyCounter>();
    if (counterScript) {
        counterScript->StopCounter();
    }
    m_skillTime = 0.f;
    m_isSkillComplete = false;

    cout << "니키 카운터 State탈출!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
}

bool NickyCounterState::CanTransitionTo(PlayerStateType newState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if (m_isSkillComplete && (newState == PlayerStateType::Wait || newState == PlayerStateType::Run))
    {
        return true;
    }
    return false;
}

