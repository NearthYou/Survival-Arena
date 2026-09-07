#include "pch.h"
#include "NickyRSkill.h"

#include "Player.h"
#include "AnimationState.h"
#include "NickyAnimRState.h"
#include "Monster.h"

#include "UIResourceManager.h"

NickyRSkill::NickyRSkill(shared_ptr<Player> _player)
	: Super(_player, 3)
{
	m_skillCooldown = 10.f;
    ConfigureProgression(70);

	SetMaxSkillLevel(3);

	m_skillImage = RESOURCES->GetOrAddTexture(L"NickyR", L"..\\Resources\\Textures\\UI\\SkillIcon\\SkillIcon_1033500.png");
	//FX_BI_Nicky_S005_Skill04_01
	//shared_ptr<Material> m_rSkillEffectImage1 = UIResourceManager::GetInstance()->LoadUIMaterial(L"NickyRSkillEffect_1", L"..\\Resources\\Textures\\Nicky\\FX_BI_Nicky_S005_Skill04_01.png");
	//shared_ptr<Material> m_rSkillEffectImage2 = UIResourceManager::GetInstance()->LoadUIMaterial(L"NickyRSkillEffect_2", L"..\\Resources\\Textures\\Nicky\\FX_BI_Nicky_S005_Skill04_02.png");


	
	shared_ptr<Material> m_rSkillEffectImage1 = make_shared<Material>();
	shared_ptr<Shader> shader = make_shared<Shader>(L"FOW.fx");
	m_rSkillEffectImage1->SetShader(shader);
	m_rSkillEffectImage1->SetRenderQueue(RenderQueue::Cutout);
	//m_rSkillEffectImage1->SetTransparent(true);
	m_rSkillEffectImage1->SetRenderingMode(RenderingMode::Forward);

	auto texture = RESOURCES->Load<Texture>(L"NickyRSkillEffect_1", L"..\\Resources\\Textures\\Nicky\\FX_BI_Nicky_S005_Skill04_01.png");
	m_rSkillEffectImage1->SetDiffuseMap(texture);

	MaterialDesc& desc = m_rSkillEffectImage1->GetMaterialDesc();
	desc.ambient = Vec4(1.f);
	desc.diffuse = Vec4(1.f);
	desc.specular = Vec4(1.f);

	RESOURCES->Add(L"NickyRSkillEffect_1", m_rSkillEffectImage1);
	


	shared_ptr<Material> m_rSkillEffectImage2 = make_shared<Material>();
	shader = make_shared<Shader>(L"FOW.fx");
	m_rSkillEffectImage2->SetShader(shader);
	m_rSkillEffectImage2->SetRenderQueue(RenderQueue::Cutout);
	//m_rSkillEffectImage1->SetTransparent(true);
	m_rSkillEffectImage2->SetRenderingMode(RenderingMode::Forward);

	auto texture2 = RESOURCES->Load<Texture>(L"NickyRSkillEffect_2", L"..\\Resources\\Textures\\Nicky\\FX_BI_Nicky_S005_Skill04_02.png");
	m_rSkillEffectImage2->SetDiffuseMap(texture2);

	desc = m_rSkillEffectImage2->GetMaterialDesc();
	desc.ambient = Vec4(1.f);
	desc.diffuse = Vec4(1.f);
	desc.specular = Vec4(1.f);

	RESOURCES->Add(L"NickyRSkillEffect_2", m_rSkillEffectImage2);
	


	{
		m_effect1 = make_shared<GameObject>();
		m_effect1->AddComponent(make_shared<MeshRenderer>());
		m_effect1->SetName(L"Nicky_RSkill_Punch_1");

		m_effect1->GetMeshRenderer()->SetMaterial(m_rSkillEffectImage1);
		m_effect1->GetMeshRenderer()->SetMesh(RESOURCES->Get<Mesh>(L"Quad"));
		m_effect1->GetMeshRenderer()->SetPass(0);
		m_effect1->GetMeshRenderer()->GetMaterial()->SetCastShadow(false);

		m_effect1->SetActive(false);

		m_effect1->GetTransform()->SetScale(Vec3(2.f, 2.f, 2.f));
		m_effect1->GetTransform()->SetLocalRotation(Vec3(90.f, 0.f, 0.f));
		m_effect1->GetTransform()->SetParent(m_playerObject->GetTransform());
		m_effect1->GetTransform()->SetLocalPosition(Vec3(-1.f, 0.f, -0.5f));

		m_effectDuration = _player->GetModelAnimator()->GetAnimationDuration(L"Skill_04_Attack");

		CURSCENE->Add(m_effect1);
	}

	{
		m_effect2 = make_shared<GameObject>();
		m_effect2->AddComponent(make_shared<MeshRenderer>());
		m_effect2->SetName(L"Nicky_RSkill_Punch_2");
				
		m_effect2->GetMeshRenderer()->SetMaterial(m_rSkillEffectImage2);
		m_effect2->GetMeshRenderer()->SetMesh(RESOURCES->Get<Mesh>(L"Quad"));
		m_effect2->GetMeshRenderer()->SetPass(0);
		m_effect2->GetMeshRenderer()->GetMaterial()->SetCastShadow(false);
				
		m_effect2->SetActive(false);
			
		m_effect2->GetTransform()->SetScale(Vec3(2.f,2.f, 2.f));
		m_effect2->GetTransform()->SetLocalRotation(Vec3(90.f, 0.f, 0.f));
		m_effect2->GetTransform()->SetParent(m_playerObject->GetTransform());
		m_effect2->GetTransform()->SetLocalPosition(Vec3(1.f, 0.f, -0.5f));


		CURSCENE->Add(m_effect2);
	}
}

NickyRSkill::~NickyRSkill()
{

}

void NickyRSkill::PlaySkill()
{
	if (!m_target)
	{
		cout << "R 스킬: 타겟이 설정되지 않았습니다." << endl;
		return;
	}

	// 초기 타겟 위치 저장
	m_lastTargetPos = m_target->GetTransform()->GetPosition();

	int soundIdx = rand() % soundCount + 1;
	wstring soundString = L"Nicky/Nicky_PlaySkill4_" + to_wstring(soundIdx) + L".wav";
	SOUND->PlaySound(soundString, 1, 0.5f);
	SOUND->PlaySound(L"Nicky/Nicky_skill04_Ready.wav", 4, 0.5f);

	CalculateSkillDirection();
}

void NickyRSkill::Update()
{
	UpdateSkillCoolDown();
	//UpdateEffectPosition();
	if (m_moveFlag)
	{
		if (IsRushAnimationPlaying())
		{
			m_rushSoundDuration += DT;
			m_moveElapsedTime += DT;

			// **핵심**: 동적 타겟 추적 로직
			if (m_isDynamicTracking && m_target)
			{
				m_trackingTimer += DT;

				// 일정 간격으로만 업데이트 (렉 방지)
				if (m_trackingTimer >= m_trackingUpdateInterval)
				{
					UpdateTargetPosition();
					m_trackingTimer = 0.f;
				}
			}

			float moveT = m_moveElapsedTime / m_moveDuration;

			if (m_rushSoundDuration > 0.1f)
			{
				SOUND->PlaySound(L"Nicky/Nicky_skill04_Rush.wav", 2, 0.5f);
				m_rushSoundDuration = 0.f;
			}

			// 이동 완료 체크
			if (moveT >= 1.0f)
			{
				moveT = 1.0f;
				m_moveFlag = false;
				SOUND->PlaySound(L"Nicky/Nicky_skill04_Attack.wav", 3, 0.5f);

				m_effect1->SetActive(true);
				m_effect2->SetActive(true);


				if (m_target && m_target->GetType() == OBJECTTYPE::MONSTER)
				{
					static_pointer_cast<Monster>(m_target)->Damaged(m_playerObject,
						static_pointer_cast<Player>(m_playerObject)->GetStatus().hitAttack * 1.5f * m_playerObject->GetSkill(3)->GetDamageMultiplier());
				}

				SkillEnd();
			}

			Vec3 curPos = Utils::Lerp(m_startPos, m_targetPos, moveT);
			m_playerObject->GetTransform()->SetPosition(curPos);
		}
		else if (m_moveElapsedTime > 0.0f)
		{
			m_moveFlag = false;
			
		}
	}
	else
	{
		
		m_effectTime += DT;
		if (m_effectTime >= m_effectDuration - 0.05f)
		{
			m_effect1->SetActive(false);
			m_effect2->SetActive(false);
			m_effectTime = 0.f;
		}
	}
}

void NickyRSkill::CalculateSkillDirection()
{
	if (!m_target)
	{
		cout << "타겟이 없어서 방향 계산 불가" << endl;
		return;
	}

	// 타겟 위치로 방향 설정
	Vec3 playerPos = m_playerObject->GetTransform()->GetPosition();
	Vec3 targetPos = m_target->GetTransform()->GetPosition();

	Vec3 direction = targetPos - playerPos;
	direction.Normalize();

	// 적절한 거리로 이동 (타겟 바로 앞까지)
	float moveDistance = Vec3::Distance(playerPos, targetPos) - 1.f; // 1미터 앞에서 정지
	if (moveDistance < 0) moveDistance = 1.0f; // 최소 이동거리

	m_startPos = playerPos;
	m_targetPos = playerPos + direction * moveDistance;

	float distance = Vec3::Distance(m_startPos, m_targetPos);
	m_moveDuration = distance / m_speed;
	
	SetRushDuration(m_moveDuration);
	cout << "러쉬 시간 : " << m_moveDuration << endl;

	m_moveElapsedTime = 0.f;
	m_moveFlag = true;

	// 회전 계산 및 적용
	float targetYaw = atan2(direction.x, direction.z) + 3.141592f;
	Vec3 currentRotation = m_playerObject->GetTransform()->GetLocalRotation();
	Vec3 newRotation = Vec3(currentRotation.x, targetYaw * 180.0f / 3.14159f, currentRotation.z);

	m_playerObject->GetTransform()->SetLocalRotation(newRotation);
}

// 첫 번째 애니메이션이 재생 중인지 확인하는 함수 추가
bool NickyRSkill::IsRushAnimationPlaying()
{
	shared_ptr<AnimationState> curAnimState = m_playerObject->GetAnimationStateMachine()->GetState(AnimationStateType::Skill_4);

	return static_pointer_cast<NickyAnimRState>(curAnimState)->IsRushAnimationActive();

	return false;
}

void NickyRSkill::SetRushDuration(float duration)
{
	m_playerObject->GetModelAnimator()->SetSequenceAnimationDuration(L"Skill_4_Sequence", 2, duration);
}

void NickyRSkill::UpdateTargetPosition()
{
	if (!m_target || !m_target->GetActive())
		return;

	Vec3 currentTargetPos = m_target->GetTransform()->GetPosition();

	// 타겟이 실제로 이동했는지 확인 (불필요한 계산 방지)
	float distanceMoved = Vec3::Distance(currentTargetPos, m_lastTargetPos);
	if (distanceMoved < 0.1f)  // 0.1미터 이하 움직임은 무시
		return;

	Vec3 playerPos = m_playerObject->GetTransform()->GetPosition();
	Vec3 direction = currentTargetPos - playerPos;
	direction.Normalize();

	// 타겟 바로 앞 1미터까지만 이동
	float moveDistance = Vec3::Distance(playerPos, currentTargetPos) - 1.f;
	if (moveDistance < 0.5f) moveDistance = 0.5f;  // 최소 거리 보장

	// **부드러운 전환**: 기존 목표점과 새 목표점을 보간
	Vec3 newTargetPos = playerPos + direction * moveDistance;
	m_targetPos = Utils::Lerp(m_targetPos, newTargetPos, 0.3f);  // 30% 보간으로 부드럽게

	// 회전도 업데이트
	float targetYaw = atan2(direction.x, direction.z) + 3.141592f;
	Vec3 currentRotation = m_playerObject->GetTransform()->GetLocalRotation();
	Vec3 newRotation = Vec3(currentRotation.x, targetYaw * 180.0f / 3.14159f, currentRotation.z);
	m_playerObject->GetTransform()->SetLocalRotation(newRotation);

	m_lastTargetPos = currentTargetPos;  // 이전 위치 저장
}

void NickyRSkill::UpdateEffectPosition()
{
	if (!m_target) return;

	//Vec3 playerPos = m_playerObject->GetTransform()->GetPosition();
	//Vec3 playerRot = m_playerObject->GetTransform()->GetRotation();

	//m_effect1->GetTransform()->SetPosition(playerPos);


	//// 플레이어 회전 행렬 생성
	//Matrix rotationMatrix = Matrix::CreateRotationY(XMConvertToRadians(playerRot.y));
	//rotationMatrix *= Matrix::CreateRotationX(XMConvertToRadians(playerRot.x));
	//rotationMatrix *= Matrix::CreateRotationZ(XMConvertToRadians(playerRot.z));

	//Vec3 rotatedOffset = Vec3::Transform(baseOffset, rotationMatrix);

	//// Collider의 오프셋 업데이트
	//m_effect1->GetTransform()->SetRotation()
}
