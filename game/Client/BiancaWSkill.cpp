#include "pch.h"
#include "Player.h"
#include "BiancaWSkill.h"

#include "ModelAnimator.h"

#include "PlayerStateMachine.h"
#include "AnimationStateMachine.h"

BiancaWSkill::BiancaWSkill(shared_ptr<Player> _player)
	: Super(_player, 1)
{
	m_player = _player;

	//Bianca Coffin Object
	m_shader = _player->GetShader();
	m_skillCooldown = 10.f;
	m_skillName = L"짧은 안식";
	m_skillDesc = L"비앙카가 정신 집중을 하며 최대 3초 동안 관속으로 잠시 몸을 피합니다.";
	m_curSkillLevel = 0;
	m_maxSkillLevel = 5;
	//Coffin 모델 생성. 
	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"BiancaCoffin", L"Bianca/BiancaCoffin_mesh");
		m2->ReadMaterial(L"Bianca/BiancaCoffin_mesh");

		auto obj = make_shared<GameObject>();
		obj->SetName(L"Bianca_Coffin");
		//obj->GetTransform()->SetParent(_player->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(0, 0, 0));
		obj->GetTransform()->SetLocalScale(Vec3(2.f));
		obj->SetActive(false);
		obj->AddComponent(make_shared<ModelRenderer>(m_shader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}
		m_coffin = obj;
		CURSCENE->Add(m_coffin);
	}
	

	{
		m_skillImage = RESOURCES->GetOrAddTexture(L"BiancaW", L"..\\Resources\\Textures\\UI\\SkillIcon\\SkillIcon_1042300.png");
	}
}

BiancaWSkill::~BiancaWSkill()
{
}

void BiancaWSkill::PlaySkill()
{
	if (m_isPlaying) {
		//너무 빠르게 다시 눌러 해제되는 것 방지. 
		if (m_repeatKey < 0.25f)
			return;
		//이미 실행중일 경우 -> W스킬 끝내기. 
		//스킬 종료. 
		m_coffin->SetActive(false);
		m_isPlaying = false;

		PlayerStatus status = m_playerObject->GetStatus();
		m_playerObject->SetDefense(status.defense - 50);
		m_repeatKey = 0.f;
		m_elapsedTime = 0.f;

		//사운드 출력
		SOUND->PlaySound(m_soundEnd, 1, 0.5f);
		SkillEnd();
	}
	else if(m_isPlaying == false && m_skillcurCooldown <= 0){
		//실행중이지 않으며, 남아있는 쿨타임이 없을때. 
		//다시 말해 스킬 실행. 
		m_coffin->SetActive(true);
		m_isPlaying = true;

		PlayerStatus status = m_playerObject->GetStatus();
		m_playerObject->SetDefense(status.defense + 50);
		SOUND->PlaySound(m_soundStart, 1, 0.5f);
	}
}

void BiancaWSkill::Update()
{
	m_skillTimer += DT;
	if (m_skillTimer >= m_skillDuration)
	{
		//m_player->GetAnimationStateMachine()->ChangeState(AnimationStateType::Wait);
		//m_player->GetPlayerStateMachine()->ChangeState(PlayerStateType::Wait);
	
		m_skillTimer = 0.f;

		return;
	}


	if (m_coffin)
		m_coffin->GetTransform()->SetPosition(m_playerObject->GetTransform()->GetPosition());

	if (m_isPlaying) {
		m_elapsedTime += DT;
		m_repeatKey += DT;
		if (m_elapsedTime >= m_duration) {
			PlaySkill();
		}
	}
	else {
		m_skillcurCooldown -= DT;
	}
}
