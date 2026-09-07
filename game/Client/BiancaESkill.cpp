#include "pch.h"
#include "BiancaESkill.h"
#include "BiancaESkillCircle.h"
#include "Player.h"
#include "Utils.h"
#include "NavMeshAgent.h"
#include "Monster.h"

#include "BiancaEState.h"
#include "BiancaAnimEState.h"

BiancaESkill::BiancaESkill(shared_ptr<Player> _player)
	: Super(_player, 2)
{
	{
		m_skillImage = RESOURCES->GetOrAddTexture(L"BiancaE", L"..\\Resources\\Textures\\UI\\SkillIcon\\SkillIcon_1042400.png");
	}

	{
		m_skillCooldown = 3.f;
		m_skillName = L"순환.";
		m_skillDesc = L"비앙카가 혈액의 고리를 충전하고 전방으로 돌진하여 스킬 피해를 입힙니다.";
		m_curSkillLevel = 0;
		m_maxSkillLevel = 5;
		m_skillImage = RESOURCES->GetOrAddTexture(L"BiancaE", L"..\\Resources\\Textures\\UI\\SkillIcon\\SkillIcon_1042400.png");
	}
	//Coffin 모델 생성. 
	{
		m_circle = make_shared<BiancaESkillCircle>();
		auto FXShader = make_shared<Shader>(L"BiancaEShader.fx");
		m_circle->AddComponent(make_shared<SnowBillboard>(Vec3(0, 0, 0), Vec3(1.f, 0.05, 1.f), 20));

		{
			shared_ptr<Material> material = make_shared<Material>();
			material->SetShader(FXShader);
			auto texture = RESOURCES->Load<Texture>(L"Bianca_ETexture", L"..\\Resources\\Textures\\veigar.jpg");
			material->SetDiffuseMap(texture);
			MaterialDesc& desc = material->GetMaterialDesc();
			desc.ambient = Vec4(1.f);
			desc.diffuse = Vec4(1.f);
			desc.specular = Vec4(1.f);
			RESOURCES->Add(L"Bianca_EMaterial", material);
			m_circle->GetSnowBillboard()->SetMaterial(material);
			m_circle->GetSnowBillboard()->SetParticleScale(Vec2(0.2f, 0.7f));
		}

		m_circle->GetTransform()->SetLocalScale(Vec3(1.f, 0.03f, 1.f));

		m_collider = make_shared<SphereCollider>();
		m_circle->AddComponent(m_collider);

		//
		m_collider->SetOffsetScale(Vec3(1.f, 1.f, 1.f));
		//

		m_circle->SetActive(false);
		m_circle->GetTransform()->SetParent(m_playerObject->GetTransform());
		m_circle->GetTransform()->SetLocalPosition(Vec3(0.f, 0.03f, 0.f));
		m_circle->GetCollider()->SetVisible(false);
		CURSCENE->Add(m_circle);
	}


}

BiancaESkill::~BiancaESkill()
{
}

void BiancaESkill::PlaySkill()
{

}

void BiancaESkill::Update()
{

	if (m_curSkillLevel <= 0)
		return;
	m_skillcurCooldown -= DT;

	//원이 작아지는 연출은 스킬 쿨에 상관없음. 
	if (m_endFlag) {
		if (m_eSkillEndElapsedTime < 0.3f) {
			m_eSkillEndElapsedTime += DT;

			float scaleT = Utils::FLerp(3.f, 2.3f, m_eSkillEndElapsedTime / 0.3f);
			Vec3 scale = Vec3(scaleT, 0.02f, scaleT);

			//cout << "startPos : " << scale.x << " " << scale.y << " " << scale.z << "\n";
			m_circle->GetTransform()->SetLocalScale(scale);

		}
		else {
			m_endFlag = false;
			m_eSkillEndElapsedTime = 0.f;
			m_circle->SetActive(false);
		}
	}

	//쿨타임 게산. 
	if (m_skillcurCooldown > 0.f)
		return;

	if (INPUT->GetButtonDown(KEY_TYPE::E) && !INPUT->GetButton(KEY_TYPE::LCTRL)) {
		//17 eID사용.
		m_circle->SetActive(true);
		
		SOUND->PlaySound(L"Bianca/Bianca_Skill03_Charge.wav", 17, 0.5f);
	}

	if (INPUT->GetButton(KEY_TYPE::E) && !INPUT->GetButton(KEY_TYPE::LCTRL)) {
		m_circleSizeElapedTime = m_circleKeepElapedTime += DT;

		float circleSize = 1.f + (m_circleSizeElapedTime / m_circleSizeDuration) * 2.f;
		if (circleSize >= 3.f) circleSize = 3.f;
		Vec3 scale = Vec3(circleSize, 0.02f, circleSize);

		//cout << "startPos : " << scale.x << " " << scale.y << " " << scale.z << "\n";
		
		m_circle->GetTransform()->SetLocalScale(scale);

	}
	else if((INPUT->GetButtonUp(KEY_TYPE::E) || m_circleKeepElapedTime > 4.f) && !INPUT->GetButton(KEY_TYPE::LCTRL))
	{
		m_playerObject->GetNavMeshAgent()->Stop();
		//E키 떼었을 때. 
		//얼마나 나갈지 세팅.
		float range =  m_maxRange * min(1.f, m_circleKeepElapedTime / m_circleSizeDuration);
		POINT mousePos = INPUT->GetMousePos();

		XMVECTOR mouseWorldPos = ScreenToWorld(mousePos);

		XMVECTOR playerPos = m_playerObject->GetTransform()->GetPosition();
		XMVECTOR direction = XMVector3Normalize(mouseWorldPos - playerPos);
		XMVECTOR skillTargetPos = XMVectorAdd(playerPos, XMVectorScale(direction, range));
		m_startPos = m_playerObject->GetTransform()->GetPosition();
		m_targetPos = skillTargetPos;

		//cout << "startPos : " << m_startPos.x << " " << m_startPos.y << " " << m_startPos.z << "\n";
		//cout << "m_targetPos : " << m_targetPos.x << " " << m_targetPos.y << " " << m_targetPos.z << "\n";

		float distance = Vec3::Distance(m_startPos, m_targetPos);
		m_moveDuration = distance / m_speed;


		
		m_moveElapsedTime = 0.f;
		// 회전 계산 및 적용
		float targetYaw = atan2(XMVectorGetX(direction), XMVectorGetZ(direction)) + 3.141592f; //3.141592 더해야 방향 제대로 됨
		Vec3 currentRotation = m_playerObject->GetTransform()->GetLocalRotation();
		Vec3 newRotation = Vec3(currentRotation.x, targetYaw * 180.0f / 3.14159f, currentRotation.z);
		m_playerObject->GetTransform()->SetLocalRotation(newRotation);
		m_circle->DamageFlag(true);
		m_moveFlag = true;
		SOUND->StopSound(17);
		SOUND->PlaySound(L"Bianca/Bianca_Skill03_Attack.wav", 17, 0.5f);
	}
	
	if (m_moveFlag) 
	{
		if (m_moveElapsedTime <= m_moveDuration)
		{
			m_moveElapsedTime += DT;
			float movet = m_moveElapsedTime / m_moveDuration;
			Vec3 curPos = Utils::Lerp(m_startPos, m_targetPos, movet);
			m_playerObject->GetTransform()->SetPosition(curPos);
			m_circle->DamageFlag(true);
		}
		else
		{
			m_moveFlag = false;
			m_circleSizeElapedTime = 0.f;
			m_circleKeepElapedTime = 0.f;
			m_moveDuration = 0.f;
			m_moveElapsedTime = 0.f;
			m_startPos = m_playerObject->GetTransform()->GetPosition();
			int SkillDamage = m_playerObject->GetStatus().hitAttack * 1.8f;

			auto objects = m_circle->GetCollisionObjects();

			for (auto object : objects) 
			{
				auto monster = dynamic_pointer_cast<Monster>(object);
				if (monster != nullptr) 
				{
					monster->Damaged(m_playerObject, SkillDamage);
					SOUND->PlaySound(L"Bianca/Bianca_Skill03_Hit.wav", 17, 0.5f);
				}
			}

			m_circle->DamageFlag(false);
			
			ForceEndSkill();


			SkillEnd();
			//m_circle->SetActive(false);
			m_endFlag = true;
			m_eSkillEndElapsedTime = 0.f;
		}
	}
}



void BiancaESkill::ForceEndSkill()
{
	cout << "E 스킬 강제 종료!" << endl;

	// 이동 중지
	m_moveFlag = false;
	m_isForceEnded = true;

	// 애니메이션을 End로 강제 변경
	if (m_playerObject->GetAnimationStateMachine())
	{
		auto animQState = static_pointer_cast<BiancaAnimEState>(
			m_playerObject->GetAnimationStateMachine()->GetState(AnimationStateType::Skill_3)
		);

		auto logicQState = static_pointer_cast<BiancaEState>(
			m_playerObject->GetPlayerStateMachine()->GetState(PlayerStateType::Skill_3)
		);

		if (animQState)
		{
			animQState->ForceEndAnimation();
		}

		if (logicQState)
		{
			logicQState->ForceEnd();
		}
	}

	m_moveFlag = false;
	m_moveDuration = 0.f;
	m_moveElapsedTime = 0.f;
	m_startPos = m_playerObject->GetTransform()->GetPosition();

	//m_skillFlag = false;

	SkillEnd();
}