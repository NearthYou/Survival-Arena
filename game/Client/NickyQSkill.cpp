#include "pch.h"
#include "NickyQSkill.h"

#include "Player.h"
#include "NickyAnimQState.h"
#include "NavMeshAgent.h"

#include "ModelAnimator.h"

#include "NickyQState.h"
#include "Nicky.h"

NickyQSkill::NickyQSkill(shared_ptr<Player> _player)
	: Super(_player, 0)
{
	m_skillCooldown = 5.f;
    ConfigureProgression(30);
	m_skillImage = RESOURCES->GetOrAddTexture(L"NickyQ", L"..\\Resources\\Textures\\UI\\SkillIcon\\SkillIcon_1033200.png");


	{
		auto snowShader = make_shared<Shader>(L"GatherBillboard.fx");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"Nicky_Charging");
		
		obj->AddComponent(make_shared<SnowBillboard>(Vec3(0, 0, 0), Vec3(1, 1, 1), 50));
		{
			// Material
			{
				shared_ptr<Material> material = make_shared<Material>();
				material->SetShader(snowShader);
				//auto texture = RESOURCES->Load<Texture>(L"Veigar", L"..\\Resources\\Textures\\grass.png");
				auto texture = RESOURCES->Load<Texture>(L"Veigar", L"..\\Resources\\Textures\\veigar.jpg");
				material->SetDiffuseMap(texture);
				MaterialDesc& desc = material->GetMaterialDesc();
				desc.ambient = Vec4(1.f);
				desc.diffuse = Vec4(1.f, 0.f, 0.f, 1.f);
				desc.specular = Vec4(1.f);
				RESOURCES->Add(L"Veigar", material);

				obj->GetSnowBillboard()->SetMaterial(material);
				obj->GetSnowBillboard()->SetParticleScale(Vec2(0.6f, 0.4f));
			}
		}
		obj->SetActive(false);
		m_chargingEffect = obj;
		CURSCENE->Add(obj);
	}
}

NickyQSkill::~NickyQSkill()
{

}

void NickyQSkill::PlaySkill()
{
	int soundIdx = rand() % soundCount + 1;
	wstring soundString = L"Nicky/Nicky_PlaySkill1_" + to_wstring(soundIdx) + L".wav";

	SOUND->PlaySound(soundString, 1, 0.5f);

	m_skillFlag = true;
    SOUND->PlaySound(L"Nicky/Nicky_skill01_Charge.wav", 2, 0.5f);
    m_chargingEffect->SetActive(true);
    m_bskillStart = true;
    m_duration = 0.f;
	// 플레이어의 데미지 플래그 리셋
	if (auto nicky = static_pointer_cast<Nicky>(m_playerObject))
	{
		nicky->m_qSkillDamageDealt = false;  // friend로 접근하거나 public 함수로 만들어야 함
	}
}

void NickyQSkill::Update()
{
	UpdateSkillCoolDown();

	Vec3 forearmPos = m_playerObject->GetModelAnimator()->GetAnimatedBonePosition(L"Bip001 R Forearm");

	// 월드 좌표계로 변환
	Vec3 worldPos = Vec3::Transform(forearmPos, m_playerObject->GetTransform()->GetWorldMatrix());

	m_chargingEffect->GetTransform()->SetPosition(worldPos);

	if (m_skillFlag)
	{


		if (m_bskillStart)
		{
			m_duration += DT;
		}

		if (INPUT->GetButtonUp(KEY_TYPE::Q))
		{
			m_chargingEffect->SetActive(false);
			SOUND->StopSound(2);
			SOUND->PlaySound(L"Nicky/Nicky_skill01_Shoot.wav", 3, 0.5f);
			m_playerObject->GetNavMeshAgent()->Stop();
			CalculateSkillDirection();
			m_duration = 0.f;
		}

		if (m_moveFlag)
		{
			//cout << "스킬에서의 이동시간 : " << m_moveDuration << endl;
			if (m_moveElapsedTime <= m_moveDuration && IsFirstAnimationPlaying())
			{
				m_moveElapsedTime += DT;
				float movet = m_moveElapsedTime / m_moveDuration;
				//cout << "이동률 : " << movet * 100 << endl;
				Vec3 curPos = Utils::Lerp(m_startPos, m_targetPos, movet);
				m_playerObject->GetTransform()->SetPosition(curPos);
			}
			else
			{
				// 첫 번째 애니메이션이 끝나면 이동 중지
				m_moveFlag = false;
				m_moveDuration = 0.f;
				m_moveElapsedTime = 0.f;
				m_startPos = m_playerObject->GetTransform()->GetPosition();

				m_skillFlag = false;

				cout << "이동완료\n";

				ForceEndSkill();

				SkillEnd();
			}
		}
	}
	else
	{

	}
}


void NickyQSkill::CalculateSkillDirection()
{
	// 차징 시간에 따른 거리 계산 (0~5초 차징을 0~1로 정규화)
	float chargeRatio = min(m_duration / 5.0f, 1.0f);
	float range = m_baseRange + (m_maxChargeRange - m_baseRange) * chargeRatio;


	POINT mousePos = INPUT->GetMousePos();
	//::ScreenToClient(GAME->GetGameDesc().hWnd, &mousePos);

	XMVECTOR mouseWorldPos = ScreenToWorld(mousePos);

	XMVECTOR playerPos = m_playerObject->GetTransform()->GetPosition();
	XMVECTOR direction = XMVector3Normalize(mouseWorldPos - playerPos);

	XMVECTOR skillTargetPos = XMVectorAdd(playerPos, XMVectorScale(direction, range));
	m_startPos = m_playerObject->GetTransform()->GetPosition();
	m_targetPos = skillTargetPos;

	float distance = Vec3::Distance(m_startPos, m_targetPos);
	m_moveDuration = (distance / m_speed);
	m_moveElapsedTime = 0.f;
	m_moveFlag = true;

	// 회전 계산 및 적용
	float targetYaw = atan2(XMVectorGetX(direction), XMVectorGetZ(direction)) + 3.141592f; //3.141592 더해야 방향 제대로 됨

	//cout << "TargetYaw : " << targetYaw * 57.2958f << "\n";
	Vec3 currentRotation = m_playerObject->GetTransform()->GetLocalRotation();
	Vec3 newRotation = Vec3(currentRotation.x, targetYaw * 180.0f / 3.14159f, currentRotation.z);

	m_playerObject->GetTransform()->SetLocalRotation(newRotation);
}

// 첫 번째 애니메이션이 재생 중인지 확인하는 함수 추가
bool NickyQSkill::IsFirstAnimationPlaying()
{
	shared_ptr<AnimationState> curAnimState = m_playerObject->GetAnimationStateMachine()->GetState(AnimationStateType::Skill_1);

	return static_pointer_cast<NickyAnimQState>(curAnimState)->IsFirstAnimationActive();

	auto animator = m_playerObject->GetModelAnimator();
	if (!animator) return false;

	wstring currentAnim = animator->GetCurrentAnimationTag();

	// 시퀀스가 재생 중이고, 현재 애니메이션이 Rush인 경우만 true
	return animator->IsSequencePlaying() && currentAnim == L"Skill_01_Rush";
	return false;
}


void NickyQSkill::ForceEndSkill()
{
	cout << "Q 스킬 강제 종료!" << endl;

	// 이동 중지
	m_moveFlag = false;
	m_isForceEnded = true;

	// 애니메이션을 End로 강제 변경
	if (m_playerObject->GetAnimationStateMachine())
	{
		auto animQState = static_pointer_cast<NickyAnimQState>(
			m_playerObject->GetAnimationStateMachine()->GetState(AnimationStateType::Skill_1)
		);

		auto logicQState = static_pointer_cast<NickyQState>(
			m_playerObject->GetPlayerStateMachine()->GetState(PlayerStateType::Skill_1)
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

	m_skillFlag = false;

	SkillEnd();
}