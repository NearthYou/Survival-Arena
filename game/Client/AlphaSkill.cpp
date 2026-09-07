#include "pch.h"
#include "Alpha.h"
#include "AlphaSkill.h"
#include "Player.h"
#include "BiancaESkillCircle.h"

AlphaSkill::AlphaSkill(shared_ptr<GameObject> _alpha) : m_alpha(_alpha)
{
	for (int idx = 0; idx < 5; ++idx) {
		m_circleObjects[idx] = make_shared<BiancaESkillCircle>();

		m_circleObjects[idx]->GetTransform()->SetScale(Vec3(1.f, 0.03f, 1.f));
		m_circleObjects[idx]->AddComponent(make_shared<SphereCollider>());
		m_circleObjects[idx]->GetCollider()->SetOffsetScale(Vec3(1.f, 30.f, 1.f));
		m_circleObjects[idx]->SetActive(false);

		CURSCENE->Add(m_circleObjects[idx]);
	}
}

AlphaSkill::~AlphaSkill()
{
}

void AlphaSkill::Start()
{
}

void AlphaSkill::Play(shared_ptr<GameObject> _target)
{
	m_isActive = true;
	m_boozer = false;
	SOUND->PlaySound(L"Alpha/AlphaOmega_skill02_Ready.wav", 22, 0.5f);

	//m_circleObject들 여러 개 배치. 
	Vec3 alphaPosition = m_alpha->GetTransform()->GetPosition();
	Vec3 targetPosition = _target->GetTransform()->GetPosition();

	Vec3 alphaToTarget = targetPosition - alphaPosition;
	alphaToTarget.y = 0.f;
	alphaToTarget.Normalize();

	for (int idx = 0; idx < 5; ++idx) {
		m_circleObjects[idx]->DamageFlag(true);

		float randomAngle = (rand() % 1200 - 600) * 0.1f;
		float angleRadians = randomAngle * (3.14159f / 180.f);
		
		float randomDistance = 1.5f + (rand() % 851) * 0.01f;  // 1.5~10.0

		Vec3 direction;
		direction.x = alphaToTarget.x * cos(angleRadians) - alphaToTarget.z * sin(angleRadians);
		direction.z = alphaToTarget.x * sin(angleRadians) + alphaToTarget.z * cos(angleRadians);

		Vec3 finalPos = alphaPosition + direction * randomDistance;
		finalPos.y = alphaPosition.y;

		m_circleObjects[idx]->GetTransform()->SetPosition(finalPos);
		cout << idx << " Circle : " << finalPos.x << " " << finalPos.y << " " << finalPos.z << "\n";
		m_circleObjects[idx]->SetActive(true);
		m_circleObjects[idx]->DamageFlag(true);
	}
}

void AlphaSkill::Update()
{
	if (m_isActive && m_skillElapsedTime <= m_skillDuration) {
		m_skillElapsedTime += DT;
	}
	else if(m_isActive && m_skillElapsedTime > m_skillDuration && (m_skillEffectElapsedTime <= m_skillEffectDuration)){
		if (!m_boozer) {
			SOUND->PlaySound(L"Alpha/AlphaOmega_skill02_Spout.wav", 22, 0.5f);
			unordered_set<shared_ptr<GameObject>> allobjects;
			for (int idx = 0; idx < 5; ++idx) {
				auto objects = m_circleObjects[idx]->GetCollisionObjects();

				for (auto& object : objects) {
					allobjects.insert(object);
				}
			}

			int damage = static_pointer_cast<Monster>(m_alpha)->GetMonsterStatus().adPower;
			//여기서 allobject보내서 데미지 주기. 
			for (auto object : allobjects) {
				auto player = dynamic_pointer_cast<Player>(object);
				if (player != nullptr) {
					player->Damaged(static_pointer_cast<Monster>(m_alpha), damage * 5);
					SOUND->PlaySound(L"AlphaOmega_skill02_Hit.wav", 22, 0.5f);
				}
			}

			m_boozer = true;
		}
		m_skillEffectElapsedTime += DT;
	}
	else if (m_isActive && (m_skillElapsedTime > m_skillDuration) && (m_skillEffectElapsedTime > m_skillEffectDuration)) {
		m_boozer = false;
		m_isActive = false;
		m_skillEffectElapsedTime = 0.f;
		m_skillElapsedTime = 0.f;

		for (int idx = 0; idx < 5; ++idx) {
			m_circleObjects[idx]->DamageFlag(false);
			m_circleObjects[idx]->SetActive(false);
		}
	}
}
