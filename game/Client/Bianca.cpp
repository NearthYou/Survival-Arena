#include "pch.h"
#include "Bianca.h"

#include "BiancaAnimWaitState.h"
#include "BiancaAnimRunState.h"
#include "BiancaAnimQState.h"
#include "BiancaAnimWState.h"
#include "BiancaAnimEState.h"
#include "BiancaAnimRState.h"
#include "BiancaAnimCraftState.h"
#include "BiancaAnimBaseAttackState.h"

#include "BiancaWaitState.h"
#include "BiancaRunState.h"
#include "BiancaQState.h"
#include "BiancaWState.h"
#include "BiancaEState.h"
#include "BiancaRState.h"
#include "BiancaCraftState.h"
#include "BiancaBaseAttackState.h"

#include "FogOfWar.h"

#include "PlayerStateMachine.h"
#include "BiancaQSkill.h"
#include "BiancaWSkill.h"
#include "BiancaESkill.h"
#include "BiancaRSkill.h"
#include "BiancaBaseAttack.h"

#include "PlayerInterface.h"

Bianca::Bianca(shared_ptr<Shader> _defaultShader)
{
	m_defaultShader = _defaultShader;
	SetName(L"Bianca");
} 

Bianca::~Bianca()
{
}

void Bianca::Start()
{
	InitBiancaModel();
	InitBiancaAnimation();
	InitBiancaPSM();
	InitBiancaComponent();
	InitBiancaSkill();
	InitBiancaStats();

	Super::Start();
}

void Bianca::Update()
{
	Super::Update();

	if (isStun()) {
		GetNavMeshAgent()->Stop();
		return;
	}

	/*if (INPUT->GetButtonDown(KEY_TYPE::Q)) {
		m_skills[0]->PlaySkill();
	}
	else if (INPUT->GetButtonDown(KEY_TYPE::W)) {
		m_skills[1]->PlaySkill();
	}
	else if (INPUT->GetButtonDown(KEY_TYPE::E)) {

	}
	else if (INPUT->GetButtonDown(KEY_TYPE::R)) {
		m_skills[3]->PlaySkill();
	}*/
}

void Bianca::LateUpdate()
{
	Super::LateUpdate();
}

void Bianca::FixedUpdate()
{
	Super::FixedUpdate();
}

void Bianca::OnCollision(shared_ptr<GameObject> _other)
{
}

void Bianca::OnCollisionEnter(shared_ptr<GameObject> _other)
{
}

void Bianca::OnCollisionExit(shared_ptr<GameObject> _other)
{
}

void Bianca::InitBiancaModel()
{
	m_model = make_shared<Model>();
	m_model->ReadModel(L"Bianca2/Bianca");
	m_model->ReadMaterial(L"Bianca2/Bianca");
}

void Bianca::InitBiancaAnimation()
{
	m_model->ReadAnimation(L"Wait", L"Bianca2/Bianca_wait");
	m_model->ReadAnimation(L"Run", L"Bianca2/Bianca_run");
	m_model->ReadAnimation(L"Skill_1", L"Bianca2/Bianca_skill1");
	m_model->ReadAnimation(L"Skill_2", L"Bianca2/Bianca_skill2");
	m_model->ReadAnimation(L"Skill_3_1", L"Bianca2/Bianca_skill3-1");
	m_model->ReadAnimation(L"Skill_3_2", L"Bianca2/Bianca_skill3-2");
	m_model->ReadAnimation(L"Skill_3_3", L"Bianca2/Bianca_skill3-3");
	m_model->ReadAnimation(L"Skill_4_1", L"Bianca2/Bianca_skill4");
	m_model->ReadAnimation(L"Skill_4_2", L"Bianca2/Bianca_skill4-2");
	m_model->ReadAnimation(L"Craft", L"Bianca2/Bianca_craftMetal");

	m_model->ReadAnimation(L"BaseAttack_01", L"Bianca2/Bianca_atk");
	m_model->ReadAnimation(L"BaseAttack_02", L"Bianca2/Bianca_atk2");


	AddComponent(make_shared<ModelAnimator>(m_defaultShader));
	{
		GetModelAnimator()->SetModel(m_model);
		GetModelAnimator()->SetPass(2);
	}

	//FSM 추가. 
	AddComponent(make_shared<AnimationStateMachine>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Craft,		make_shared<BiancaAnimCraftState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Wait,			make_shared<BiancaAnimWaitState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Run,			make_shared<BiancaAnimRunState>());

	GetAnimationStateMachine()->RegisterState(AnimationStateType::Skill_1,		make_shared<BiancaAnimQState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Skill_2,		make_shared<BiancaAnimWState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Skill_3,		make_shared<BiancaAnimEState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Skill_4,		make_shared<BiancaAnimRState>());

	GetAnimationStateMachine()->RegisterState(AnimationStateType::BaseAttack,	make_shared<BiancaAnimBaseAttackState>());

	auto animator = GetModelAnimator();

	// Q 스킬 시퀀스
	vector<wstring> skill1Anims = { L"Skill_1" };
	animator->CreateSequence(L"Skill_1_Sequence", skill1Anims, false);
	// W 스킬 시퀀스
	vector<wstring> skill2Anims = { L"Skill_2" };
	animator->CreateSequence(L"Skill_2_Sequence", skill2Anims, false);

	// E 스킬은 동적으로 스퀀스 생성됨

	// R 스킬 시퀀스 (Skill_04_Ready -> Skill_04_Start -> Skill_04_Attack)
	vector<wstring> skill4Anims = { L"Skill_4_1", L"Skill_4_2" };
	vector<float> skill4Durations;
	skill4Durations.push_back(animator->GetAnimationDuration(L"Skill_4_1"));
	skill4Durations.push_back(animator->GetAnimationDuration(L"Skill_4_2"));
	animator->CreateSequence(L"Skill_4_Sequence", skill4Anims, skill4Durations, false);

	// W 스킬 시퀀스
	vector<wstring> craftMotion = { L"Craft" };
	animator->CreateSequence(L"Craft_Sequence", craftMotion, false);

}

void Bianca::InitBiancaPSM()
{
	m_playerStateMachine = make_shared<PlayerStateMachine>(0);

	auto self = static_pointer_cast<Player>(shared_from_this());
	m_playerInterface = make_shared<PlayerInterface>(self);

	m_playerStateMachine->SetPlayerInterface(m_playerInterface);

	m_playerStateMachine->RegisterState(PlayerStateType::Craft,			make_shared<BiancaCraftState>(GetModelAnimator()));
	m_playerStateMachine->RegisterState(PlayerStateType::Run,			make_shared<BiancaRunState>());
	m_playerStateMachine->RegisterState(PlayerStateType::Wait,			make_shared<BiancaWaitState>());	

	m_playerStateMachine->RegisterState(PlayerStateType::Skill_1,		make_shared<BiancaQState>(GetModelAnimator()));
	m_playerStateMachine->RegisterState(PlayerStateType::Skill_2,		make_shared<BiancaWState>(GetModelAnimator()));
	m_playerStateMachine->RegisterState(PlayerStateType::Skill_3,		make_shared<BiancaEState>(GetModelAnimator()));
	m_playerStateMachine->RegisterState(PlayerStateType::Skill_4,		make_shared<BiancaRState>(GetModelAnimator()));
	m_playerStateMachine->RegisterState(PlayerStateType::BaseAttack,	make_shared<BiancaBaseAttackState>(GetModelAnimator(), shared_from_this()));

}

void Bianca::InitBiancaComponent()
{
	m_collider = make_shared<SphereCollider>();
	m_collider->SetOffset(Vec3(0, 1, 0));
	m_collider->SetOffsetScale(Vec3(1.f, 1.f, 1.f));
	m_collider->SetVisible(false);
	m_rigidbody = make_shared<Rigidbody>();
	m_navAgent = make_shared<NavMeshAgent>();

	//행동 스크립트? 컴포넌트?
	auto attackScript = make_shared<BiancaBaseAttack>();
	attackScript->SetOwner(shared_from_this());
	attackScript->SetActive(false);
	AddComponent(attackScript);

	AddComponent(m_playerStateMachine);
	AddComponent(m_collider);
	AddComponent(m_rigidbody);
	AddComponent(m_navAgent);
	AddComponent(make_shared<FogOfWar>());

	//PlayerStateMachine 객체가 준비된 이후에 Delegate 등록
	m_playerStateMachine->OnSkillUsed += [this](int skillIndex, shared_ptr<GameObject> target) {
		if (skillIndex >= 0 && skillIndex < (int)m_skills.size() && m_skills[skillIndex])
		{
			m_skills[skillIndex]->PlaySkill();
		}
	};
}

void Bianca::InitBiancaSkill()
{
	m_skills[0] = make_unique<BiancaQSkill>(static_pointer_cast<Player>(shared_from_this()));
	m_skills[1] = make_unique<BiancaWSkill>(static_pointer_cast<Player>(shared_from_this()));
	m_skills[2] = make_unique<BiancaESkill>(static_pointer_cast<Player>(shared_from_this()));
	m_skills[3] = make_unique<BiancaRSkill>(static_pointer_cast<Player>(shared_from_this()));
}

void Bianca::InitBiancaStats()
{
	//기본 세팅. 
	SetHitAttack(31.f);
	SetDefense(51);
	SetMaxHP(870);
	SetHP(870);
	SetHealing(0.3);
	SetMaxStamina(580);
	SetStamina(580);
	SetHealingStamina(3.6);
	SetHitSpeed(0.4);
	SetMoveSpeed(3.4);
	GetNavMeshAgent()->SetSpeed(3.4f);


	//성장치 세팅. 
	m_growStatus.HitAttack = 4.4;
	m_growStatus.defense = 2.8;
	m_growStatus.hp = 82;
	m_growStatus.healing = 0.03;
	m_growStatus.stamina = 14;
	m_growStatus.healing_Stamina = 0.01;
}


void Bianca::Birth()
{
	//Birth Animation 관련. 
}

void Bianca::Death()
{

}

void Bianca::MakeItem()
{

}

void Bianca::MakeFood()
{

}
