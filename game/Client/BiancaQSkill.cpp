#include "pch.h"
#include "BiancaQSkill.h"
#include "Player.h"
#include "BiancaQProjectile.h"
#include "BiancaQCone.h"

BiancaQSkill::BiancaQSkill(shared_ptr<Player> _player)
	: Super(_player, 0)
{
	{
		m_skillCooldown = 10.f;
		m_skillName = L"선혈의 투창.";
		m_skillDesc = L"비앙카가 지정한 지점에 피의 창을 던져 충돌한 대상에게 스킬 피해를 입힙니다. 피의 창은 도착 위치에서 원형으로 퍼져 충돌하는 적에게 스킬 피해를 입히고 사라집니다.";
		m_curSkillLevel = 0;
		m_maxSkillLevel = 5;
		m_skillImage = RESOURCES->GetOrAddTexture(L"BiancaQ", L"..\\Resources\\Textures\\UI\\SkillIcon\\SkillIcon_1042200.png");
	}
	{
		m_Projectile = make_shared<BiancaQProjectile>(_player);
		m_Projectile->SetName(L"Bianca_Q_Projectile");
		m_Projectile->SetActive(false);
		m_Projectile->AddComponent(make_shared<MeshRenderer>());
		m_Projectile->AddComponent(make_shared<SphereCollider>());
		
		{
			auto mesh = RESOURCES->Get<Mesh>(L"Sphere");
			m_Projectile->GetMeshRenderer()->SetMesh(mesh);
			m_Projectile->GetMeshRenderer()->SetPass(0);
			m_Projectile->GetMeshRenderer()->SetMaterial(RESOURCES->Get<Material>(L"default"));
		}
		CURSCENE->Add(m_Projectile);
	}

	{
		m_Cone = make_shared<BiancaQCone>(_player);
		m_Cone->SetName(L"Bianca_Q_Cone");
		m_Cone->SetActive(false);
		m_Cone->AddComponent(make_shared<MeshRenderer>());
		m_Cone->GetTransform()->SetScale(Vec3(2.f, 4.f, 2.f));
		m_Cone->GetTransform()->SetPosition(_player->GetTransform()->GetPosition());
		m_Cone->AddComponent(make_shared<AABBBoxCollider>());
		m_Cone->GetCollider()->SetOffsetScale(Vec3(1, 10, 1));
		m_Cone->SetType(OBJECTTYPE::MAP);
		{ 
			auto mesh = RESOURCES->Get<Mesh>(L"Cone");
			m_Cone->GetMeshRenderer()->SetMesh(mesh);
			m_Cone->GetMeshRenderer()->SetPass(0);
			auto mat = RESOURCES->Get<Material>(L"default")->Clone();
			mat->GetMaterialDesc().diffuse = Vec4(1.f, 0.5f, 0.5f, 1.f);
			m_Cone->GetMeshRenderer()->SetMaterial(mat);
		}
		CURSCENE->Add(m_Cone);
	}
}

BiancaQSkill::~BiancaQSkill()
{
}

void BiancaQSkill::PlaySkill()
{
	if (m_skillcurCooldown >= 0.f) return;
	//cout << "Bianca Q Skill 시작 !!! \n";

	POINT mousePos = INPUT->GetMousePos();
	//::ScreenToClient(GAME->GetGameDesc().hWnd, &mousePos);

	XMVECTOR mouseWorldPos = ScreenToWorld(mousePos);

	XMVECTOR playerPos = m_playerObject->GetTransform()->GetPosition();
	XMVECTOR direction = XMVector3Normalize(mouseWorldPos - playerPos);
	XMVECTOR lengthVector = XMVector3Length(mouseWorldPos - playerPos);
	float skilllength = min(m_skillRange, XMVectorGetX(lengthVector));
	XMVECTOR skillTargetPos = XMVectorAdd(playerPos, XMVectorScale(direction, skilllength));
	Vec3 startPos = m_playerObject->GetTransform()->GetPosition();
	Vec3 targetPos = skillTargetPos;

	float distance = Vec3::Distance(startPos, targetPos);
	float flightTime = distance / m_Projectile->GetSpeed();
	// 회전 계산 및 적용
	float targetYaw = atan2(XMVectorGetX(direction), XMVectorGetZ(direction)) + 3.141592f; //3.141592 더해야 방향 제대로 됨

	//cout << "TargetYaw : " << targetYaw * 57.2958f << "\n";
	Vec3 currentRotation = m_playerObject->GetTransform()->GetLocalRotation();
	Vec3 newRotation = Vec3(currentRotation.x, targetYaw * 180.0f / 3.14159f, currentRotation.z);
	m_playerObject->GetTransform()->SetLocalRotation(newRotation);

	//cout << targetPos.x << " " << targetPos.y << " " << targetPos.z << "\n";
	//투사체 생성. 날리기.
	m_Projectile->SetMoveTarget(startPos, targetPos, flightTime);

	SOUND->PlaySound(L"Bianca/Bianca_Skill01.wav", 1, 0.5f);

	//SkillEnd();
}

void BiancaQSkill::Update()
{
	m_skillcurCooldown -= DT;
	if (m_Projectile->GetArrive()) {
		//m_Projectile이 도착한 경우. Arrive를 False로 바꾸고. 
		m_Projectile->SetArrive(false);

		//그 자리에 Cone 배치.
		Vec3 ProjectilePos = m_Projectile->GetTransform()->GetPosition();
		ProjectilePos.y -= 5.f;
		//cout << ProjectilePos.x << " " << ProjectilePos.y << " " << ProjectilePos.z << "\n";
		m_Cone->GetTransform()->SetPosition(ProjectilePos);
		m_Cone->ResetTimer();
		m_Cone->SetActive(true);
	}
}

void BiancaQSkill::UpdateAnimation()
{

}
