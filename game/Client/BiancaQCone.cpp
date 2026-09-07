#include "pch.h"
#include "BiancaQCone.h"
#include "Player.h"
#include "Monster.h"

BiancaQCone::BiancaQCone(shared_ptr<GameObject> _owner)
	: m_Owner(_owner)
{
}

BiancaQCone::~BiancaQCone()
{
}

void BiancaQCone::Start()
{
}

void BiancaQCone::Update()
{
	Super::Update();
	if (!GetActive()) {
		m_upElapsedTime = 0.f;
		return;
	}

	//m_lifeTime동안만 살아남을 수 있음. 
	m_timer += DT;
	if (m_timer >= m_lifeTime) {
		SetActive(false);
		m_isBind = false;
		debugFlag = false;
		m_upElapsedTime = 0.f;
		m_targetMonster.reset();
		m_targetPlayer.reset();
	}

	if (m_isBind) {
		//CONE 솟아오르기. 
		if (m_upDuration <= m_upElapsedTime)
			return;

		m_upElapsedTime += DT;

		float t = m_upElapsedTime / m_upDuration;
		float yLerp = Utils::FLerp(m_startY, m_endY, t);
		//cout << "Pos Lerp" << m_upElapsedTime<<"\n";

		Vec3 curPos = GetTransform()->GetPosition();
		curPos.y = yLerp;
		GetTransform()->SetPosition(curPos);
	}
}

void BiancaQCone::OnCollisionEnter(shared_ptr<GameObject> _other)
{
	if (!_other || _other == m_Owner || _other->GetType() != OBJECTTYPE::MONSTER)
		return;

	const auto monster = dynamic_pointer_cast<Monster>(_other);
	if (!monster)
		return;

	Super::OnCollisionEnter(_other);
	m_targetMonster = monster;
	m_isBind = true;
	m_startY = GetTransform()->GetPosition().y;
	m_endY = m_startY + 5.5f;
	m_upElapsedTime = 0.f;
	monster->Damaged(m_Owner, static_pointer_cast<Player>(m_Owner)->GetStatus().hitAttack * 1.2f);
	SOUND->PlaySound(L"Bianca/Bianca_Skill01_End.wav", 1, 0.5f);
}
