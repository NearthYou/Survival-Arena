#include "pch.h"
#include "NickyERange.h"

#include "Monster.h"
#include "Player.h"
#include "Wolf.h"

NickyERange::NickyERange(shared_ptr<Player> _player)
{
    m_player = _player;
}

NickyERange::~NickyERange()
{

}

void NickyERange::Start()
{
	Super::Start();
}

void NickyERange::Update()
{
	Super::Update();
}

void NickyERange::OnCollisionEnter(shared_ptr<GameObject> _other)
{
   
    HandleDamage(_other);
}

void NickyERange::OnCollision(shared_ptr<GameObject> _other)
{

    HandleDamage(_other);
}

// 공통 데미지 처리 함수 추가
void NickyERange::HandleDamage(shared_ptr<GameObject> _other)
{
    if (_other->GetType() != OBJECTTYPE::MONSTER)
        return;

    // 이미 데미지를 준 몬스터인지 확인
    if (m_damagedMonsters.find(_other) != m_damagedMonsters.end()) {
        return; // 이미 처리된 몬스터
    }

    // 새로운 몬스터에게 데미지
    static_pointer_cast<Monster>(_other)->Damaged(m_player,
        static_pointer_cast<Player>(m_player)->GetStatus().hitAttack * 1.5f * m_player->GetSkill(2)->GetDamageMultiplier());

    // 데미지를 준 몬스터 목록에 추가
    m_damagedMonsters.insert(_other);

   
    SOUND->PlaySound(L"Nicky/Nicky_Skill03_Hit.wav", 3, 0.5f);
}

void NickyERange::Reset()
{
    m_damagedMonsters.clear();
}