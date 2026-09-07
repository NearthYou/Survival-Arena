#include "pch.h"
#include "BiancaQProjectile.h"
#include "Player.h"
#include "Monster.h"

BiancaQProjectile::BiancaQProjectile(shared_ptr<GameObject> _owner)
	:m_Owner(_owner)
{
}

BiancaQProjectile::~BiancaQProjectile()
{
}

void BiancaQProjectile::Start()
{
}

void BiancaQProjectile::Update()
{
	Super::Update();
	if (!m_moving || !GetActive()) return;
	//BiancaQSkill에서 SetMoveTarget로 설정해주면 바로 움직임. 
	m_elapsedTime += DT;
	Vec3 curPos = GetTransform()->GetPosition();

	if (m_elapsedTime >= m_duration) {
		SOUND->PlaySound(L"Bianca/Bianca_Skill01_Active.wav", 1, 0.5f);
		m_arrive = true;
		SetActive(false);
		m_moving = false;
		m_elapsedTime = 0.f;
	}
	// 위치 업데이트
	Vec3 newPos = curPos + m_direction * m_speed * DT;

	//cout << newPos.x << " " << newPos.y << " " << newPos.z << "\n";
	GetTransform()->SetPosition(newPos);
}

void BiancaQProjectile::OnCollision(shared_ptr<GameObject> _other)
{
}

void BiancaQProjectile::OnCollisionEnter(shared_ptr<GameObject> _other)
{
	if (m_baseAttack) return;
	if (_other->GetType() == OBJECTTYPE::PLAYER) {
		SOUND->PlaySound(L"Bianca/Bianca_Skill01_Hit02.wav", 1, 0.5f);
	}
	if (_other->GetType() == OBJECTTYPE::MONSTER) {
		static_pointer_cast<Monster>(_other)->Damaged(m_Owner, static_pointer_cast<Player>(m_Owner)->GetStatus().hitAttack * 1.f);
		SOUND->PlaySound(L"Bianca/Bianca_Skill01_Hit01.wav", 1, 0.5f);
	}
}

void BiancaQProjectile::OnCollisionExit(shared_ptr<GameObject> _other)
{
}

void BiancaQProjectile::SetMoveTarget(Vec3& _startPos, Vec3& _endPos, float _timer)
{
	m_startPos = _startPos;
	m_startPos.y += 2.5f;
	m_endPos = _endPos;
	m_endPos.y += 2.5f;
	m_direction = m_endPos - m_startPos;
	m_duration = _timer;
	GetTransform()->SetPosition(m_startPos);
	m_direction.Normalize();

	m_moving = true;
	SetActive(true);
}
