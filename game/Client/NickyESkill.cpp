#include "pch.h"
#include "Player.h"
#include "NickyESkill.h"
#include "NickyERange.h"
#include "PlayerStateMachine.h"

#include "BaseCollider.h"
#include "AABBBoxCollider.h"

NickyESkill::NickyESkill(shared_ptr<Player> _player)
	: Super(_player, 2)
{
	m_shader = _player->GetShader();
	m_skillCooldown = 5.f;
    ConfigureProgression(40);
	m_skillName = L"강력한 펀치";
	m_skillDesc = L"니키가 강력한 펀치로 전방의 적에게 스킬 피해를 입히고 2초 동안 이동 속도를 35% 감소시킵니다.";
	m_curSkillLevel = 0;
	m_maxSkillLevel = 5;

	{
		m_skillImage = RESOURCES->GetOrAddTexture(L"NickyE", L"..\\Resources\\Textures\\UI\\SkillIcon_1033400");
	}
	{
		shared_ptr<Model> m1 = make_shared<Model>();

		m1->ReadModel(L"Nicky/NickyESkill_Mesh");
		m1->ReadMaterial(L"Nicky/NickyESkill_Mesh");

		m_skillRange = make_shared<NickyERange>(_player);
		m_skillRange->SetName(L"Nicky_E_Range");
		m_skillRange->SetActive(false);

		m_skillRange->GetTransform()->SetScale(Vec3(0.01f, 0.01f, 0.01f));
		m_skillRange->GetTransform()->SetLocalRotation(Vec3(90.f, 0.f, 0.f));

		m_skillRange->AddComponent(make_shared<SphereCollider>());

		m_skillRange->SetOwner(_player);
		m_skillRange->GetTransform()->SetParent(m_playerObject->GetTransform());

		m_skillRange->GetCollider()->SetOffset(Vec3(0.f, 1.f, -8.f));
		m_skillRange->GetCollider()->SetOffsetScale(Vec3(200.f, 200.f, 200.f));
		
		m_skillRange->AddComponent(make_shared<ModelRenderer>(m_shader));
		{
			m_skillRange->GetModelRenderer()->SetModel(m1);
			m_skillRange->GetModelRenderer()->SetPass(1);
		}

		
		CURSCENE->Add(m_skillRange);
	}
	m_skillImage = RESOURCES->GetOrAddTexture(L"NickyQ", L"..\\Resources\\Textures\\UI\\SkillIcon\\SkillIcon_1033400.png");
}

NickyESkill::~NickyESkill()
{

}

void NickyESkill::PlaySkill()
{
	cout << "Nicky E Skill 시작 !!! \n";

	if (auto eRange = dynamic_pointer_cast<NickyERange>(m_skillRange))
	{
		eRange->Reset();
	}

	m_skillRange->SetActive(true);
	m_bskillStart = true;

	CalculateSkillDirection();


	int soundIdx = rand() % soundCount + 1;
	wstring soundString = L"Nicky/Nicky_PlaySkill3_" + to_wstring(soundIdx) + L".wav";

	SOUND->PlaySound(soundString, 1, 0.5f);
	//SkillEnd();
}

void NickyESkill::Update()
{
	UpdateSkillCoolDown();

	PlayAttackSound();

	UpdateColliderPosition();

	PlayerStateType curState = m_playerObject->GetPlayerStateMachine()->GetCurrentState();
	if (curState != PlayerStateType::Skill_3)
	{
		m_skillRange->SetActive(false);
		m_duration = 0.f;
		m_bskillStart = false;
	}
	else
	{
		m_skillRange->SetActive(true);	
		m_bskillStart = true;
	}
}

void NickyESkill::PlayAttackSound()
{
	if (m_bskillStart)
	{
		m_duration += DT;
	
		if (m_duration >= 0.45f)
		{
			SOUND->PlaySound(L"Nicky/Nicky_Skill03.wav", 2, 0.5f);
			m_duration = 0.f;
		}
	}
}

void NickyESkill::UpdateColliderPosition()
{
	Vec3 playerPos = m_playerObject->GetTransform()->GetPosition();
	Vec3 playerRot = m_playerObject->GetTransform()->GetRotation();

	m_skillRange->GetTransform()->SetPosition(playerPos);

	// Collider 오프셋을 플레이어 회전에 따라 계산
	if (m_skillRange->GetCollider())
	{
		// 플레이어 회전 행렬 생성
		Matrix rotationMatrix = Matrix::CreateRotationY(XMConvertToRadians(playerRot.y));
		rotationMatrix *= Matrix::CreateRotationX(XMConvertToRadians(playerRot.x));
		rotationMatrix *= Matrix::CreateRotationZ(XMConvertToRadians(playerRot.z));

		// 기본 오프셋 (0, 1, -3)을 회전시킴
		Vec3 baseOffset = Vec3(0.f, 1.f, -8.f);
		Vec3 rotatedOffset = Vec3::Transform(baseOffset, rotationMatrix);

		// Collider의 오프셋 업데이트
		m_skillRange->GetCollider()->SetOffset(rotatedOffset);
	}
}

void NickyESkill::CalculateSkillDirection()
{
	POINT mousePos = INPUT->GetMousePos();
	//::ScreenToClient(GAME->GetGameDesc().hWnd, &mousePos);

	XMVECTOR mouseWorldPos = ScreenToWorld(mousePos);

	XMVECTOR playerPos = m_playerObject->GetTransform()->GetPosition();
	XMVECTOR direction = XMVector3Normalize(mouseWorldPos - playerPos);
	
	// 회전 계산 및 적용
	float targetYaw = atan2(XMVectorGetX(direction), XMVectorGetZ(direction)) + 3.141592f; //3.141592 더해야 방향 제대로 됨

	//cout << "TargetYaw : " << targetYaw * 57.2958f << "\n";
	Vec3 currentRotation = m_playerObject->GetTransform()->GetLocalRotation();
	Vec3 newRotation = Vec3(currentRotation.x, targetYaw * 180.0f / 3.14159f, currentRotation.z);

	m_playerObject->GetTransform()->SetLocalRotation(newRotation);
}
