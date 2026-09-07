#include "pch.h"
#include "Alpha.h"
#include "ItemBox.h"

#include "AlphaAnimAppearState.h"
#include "AlphaAnimDeathState.h"
#include "AlphaAnimDyingState.h"
#include "AlphaAnimWaitState.h"
#include "AlphaAnimWalkState.h"
#include "AlphaAnimTraceState.h"
#include "AlphaAnimAttackState.h"

#include "AlphaAppearState.h"
#include "AlphaDeathState.h"
#include "AlphaDyingState.h"
#include "AlphaWaitState.h"
#include "AlphaWalkState.h"
#include "AlphaTraceState.h"
#include "AlphaAttackState.h"

#include "AlphaTrace.h"
#include "AlphaBaseAttack.h"
#include "AlphaSkill.h"

#include "MonsterStateMachine.h"
#include "MonsterInterface.h"

#include "SkillObject.h"
#include "Player.h"

Alpha::Alpha(shared_ptr<Shader> _shader)
	: Super(_shader)
{
	SetName(L"Alpha");
	m_itembox = make_shared<ItemBox>();
}

Alpha::~Alpha()
{

}

void Alpha::Start()
{
	
	InitAlphaModel();
	InitAlphaAnimation();
	InitAlphaMSM();

	InitAlphaComponent();

	//InitAlphaAI();
	
	InitAlphaStats();

	Super::Start();


}

void Alpha::Update()
{
	if (m_skill != nullptr)
		m_skill->Update();
	Super::Update();
}

void Alpha::LateUpdate()
{
	Super::LateUpdate();
}

void Alpha::FixedUpdate()
{
	Super::FixedUpdate();
}

void Alpha::OnCollision(shared_ptr<GameObject> _other)
{
}

void Alpha::OnCollisionEnter(shared_ptr<GameObject> _other)
{
	//shared_ptr<GameObject> chaseTarget = nullptr;
	//// 만약 _other가 Player라면 (직접 충돌)
	//if (dynamic_pointer_cast<Player>(_other)) {
	//	chaseTarget = _other;
	//}
	//// 만약 _other가 스킬 오브젝트라면, owner를 찾아서 Player를 추적
	//else if (auto skillObj = dynamic_pointer_cast<SkillObject>(_other)) {
	//	if (skillObj->GetOwner()) {
	//		chaseTarget = skillObj->GetOwner();
	//	}
	//}

	//// 기타 예외 (추가 오브젝트 타입들은 필요시 확장)
	//if (chaseTarget) {
	//	//static_pointer_cast<AlphaTraceState>(m_monsterStateMachine->GetState(MonsterStateType::Trace))->SetOtherObject(chaseTarget);
	//	//static_pointer_cast<AlphaAttackState>(m_monsterStateMachine->GetState(MonsterStateType::Attack))->SetOtherObject(chaseTarget);

	//	GetComponent<AlphaBaseAttack>()->SetTarget(chaseTarget);

	//	//m_monsterStateMachine->ChangeState(MonsterStateType::Trace);
	//	//m_animationStateMachine->ChangeState(AnimationStateType::Trace);
	//}

	if (_other->GetType() == OBJECTTYPE::PLAYER ||
		(dynamic_pointer_cast<SkillObject>(_other) &&
			dynamic_pointer_cast<SkillObject>(_other)->GetOwner()->GetType() == OBJECTTYPE::PLAYER))
	{
		SetAttacked(true);
		cout << "몬스터가 공격받음" << endl;
	}
}

void Alpha::OnCollisionExit(shared_ptr<GameObject> _other)
{
	SetAttacked(false);
}

void Alpha::PlaySkill(shared_ptr<GameObject> _target)
{
	//cout << "Alpha Skill 발동!\n";
	m_skill->Play(_target);
}


void Alpha::InitAlphaModel()
{
	m_model = make_shared<Model>();
	m_model->ReadModel(L"alpha/alpha_mesh");
	m_model->ReadMaterial(L"alpha/alpha_mesh");
}

void Alpha::InitAlphaAnimation()
{
	//Appear Wait
	m_model->ReadAnimation(L"Appear", L"alpha/alpha_appear_anim");
	m_model->ReadAnimation(L"Atk1", L"alpha/alpha_atk1_anim");
	m_model->ReadAnimation(L"Atk2", L"alpha/alpha_atk2_anim");
	m_model->ReadAnimation(L"Death", L"alpha/alpha_death_anim");
	m_model->ReadAnimation(L"Dying", L"alpha/alpha_dying_anim");
	m_model->ReadAnimation(L"Run", L"alpha/alpha_walk_anim");
	m_model->ReadAnimation(L"Wait", L"alpha/alpha_wait_anim");

	m_model->ReadAnimation(L"Skill1atk", L"alpha/alpha_skill1atk_anim");
	m_model->ReadAnimation(L"Skill1ready", L"alpha/alpha_skill1ready_anim");
	m_model->ReadAnimation(L"Skill2", L"alpha/alpha_skill2_anim");

	AddComponent(make_shared<ModelAnimator>(m_defaultShader));
	{
		GetModelAnimator()->SetModel(m_model);
		GetModelAnimator()->SetPass(2);
	}
	//FSM 추가. 
	m_animationStateMachine = make_shared<AnimationStateMachine>(AnimationStateType::Appear);
	AddComponent(m_animationStateMachine);
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Wait, make_shared<AlphaAnimWaitState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Appear, make_shared<AlphaAnimAppearState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Death, make_shared<AlphaAnimDeathState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Dying, make_shared<AlphaAnimDyingState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Run, make_shared<AlphaAnimWalkState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Trace, make_shared<AlphaAnimTraceState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::BaseAttack, make_shared<AlphaAnimAttackState>());

	auto animator = GetModelAnimator();

	// 알파 등장 시퀀스. 
	vector<wstring> appear = { L"Appear" };
	vector<float> appearDuration;
	appearDuration.push_back(animator->GetAnimationDuration(L"Appear"));
	animator->CreateSequence(L"Appear", appear, appearDuration, false);


	// 알파 죽는 중 시퀀스. 
	vector<wstring> death = { L"Death" };
	vector<float> deathDuration;
	deathDuration.push_back(animator->GetAnimationDuration(L"Death"));
	animator->CreateSequence(L"Death", death, deathDuration, false);

	// 알파 시체 시퀀스. 
	vector<wstring> dying = { L"Dying" };
	vector<float> dyingDuration;
	dyingDuration.push_back(animator->GetAnimationDuration(L"Dying"));
	animator->CreateSequence(L"Dying", dying, dyingDuration, true);


	// 알파 달리기. 
	vector<wstring> runAnims = { L"Run" };
	vector<float> runAnimsDurations;
	runAnimsDurations.push_back(animator->GetAnimationDuration(L"Run"));
	animator->CreateSequence(L"Run", runAnims, runAnimsDurations, true);

	// 알파 공격모션 1
	vector<wstring> atk1Anims = { L"Atk1" };
	vector<float> atk1AnimsDurations;
	atk1AnimsDurations.push_back(animator->GetAnimationDuration(L"Atk1"));
	animator->CreateSequence(L"Alpha_Atk1_Sequence", atk1Anims, atk1AnimsDurations, false);

	// 알파 공격모션 2
	vector<wstring> atk2Anims = { L"Atk2" };
	vector<float> atk2AnimsDurations;
	atk2AnimsDurations.push_back(animator->GetAnimationDuration(L"Atk2"));
	animator->CreateSequence(L"Alpha_Atk2_Sequence", atk2Anims, atk2AnimsDurations, false);


	// 알파 스킬모션
	vector<wstring> skillAnims = { L"Skill2"};
	vector<float> skillAnimsDurations;
	skillAnimsDurations.push_back(animator->GetAnimationDuration(L"Skill2"));
	animator->CreateSequence(L"Alpha_Skill_Sequence", skillAnims, skillAnimsDurations, false);

	//// 알파 등장 시퀀스. 
	//vector<wstring> skill1 = { L"Skill1ready", L"Skill1atk" };
	//animator->CreateSequence(L"Alpha_Skill1_Sequence", skill1, false);
}

void Alpha::InitAlphaMSM()
{
	m_monsterStateMachine = make_shared<MonsterStateMachine>();
	AddComponent(m_monsterStateMachine);

	auto self = static_pointer_cast<Monster>(shared_from_this());
	m_monsterInterface = make_shared<MonsterInterface>(self);

	m_monsterStateMachine->SetMonsterInterface(m_monsterInterface);

	m_monsterStateMachine->RegisterState(MonsterStateType::Wait,	make_shared<AlphaWaitState>());
	m_monsterStateMachine->RegisterState(MonsterStateType::Appear,	make_shared<AlphaAppearState>());
	m_monsterStateMachine->RegisterState(MonsterStateType::Death, make_shared<AlphaDeathState>());
	m_monsterStateMachine->RegisterState(MonsterStateType::Dying, make_shared<AlphaDyingState>());
	m_monsterStateMachine->RegisterState(MonsterStateType::Run,	make_shared<AlphaWalkState>());

	m_monsterStateMachine->RegisterState(MonsterStateType::Trace,	make_shared<AlphaTraceState>(shared_from_this()));
	m_monsterStateMachine->RegisterState(MonsterStateType::Attack,	make_shared<AlphaAttackState>(shared_from_this()));

}

void Alpha::InitAlphaComponent()
{
	m_collider = make_shared<SphereCollider>();
	m_collider->SetOffset(Vec3(0, 1, 0));
	m_collider->SetOffsetScale(Vec3(1.f, 1.f, 1.f));
	m_collider->SetVisible(false);
	m_rigidbody = make_shared<Rigidbody>();
	m_navAgent = make_shared<NavMeshAgent>();

	//m_skill = make_shared<AlphaSkill>(shared_from_this());

	//행동 스크립트? 컴포넌트?
	auto attackScript = make_shared<AlphaBaseAttack>();
	attackScript->SetOwner(shared_from_this());
	AddComponent(attackScript);

	auto traceScript = make_shared<AlphaTrace>();
	traceScript->SetOwner(shared_from_this());
	AddComponent(traceScript);

	auto skillScript = make_shared<AlphaSkill>(shared_from_this());
	m_skill = skillScript;

	AddComponent(m_collider);
	AddComponent(m_rigidbody);
	AddComponent(m_navAgent);
	AddComponent(m_itembox);
	
}

//void Alpha::InitAlphaAI()
//{
//	auto sharedThis = dynamic_pointer_cast<Monster>(shared_from_this());
//
//	auto appearAI = make_shared<AlphaAppearAI>(sharedThis);
//	auto attackAI = make_shared<AlphaAttackAI>(sharedThis);
//	auto deathAI = make_shared<AlphaDeathAI>(sharedThis);
//	auto idleAI = make_shared<AlphaIdleAI>(sharedThis);
//
//	m_AIMap[L"Appear"] = appearAI;
//	m_AIMap[L"Attack"] = attackAI;
//	m_AIMap[L"Death"] = deathAI;
//	m_AIMap[L"Idle"] = idleAI;
//
//	m_curAI = appearAI;
//	m_curAI->Enter();
//}

void Alpha::InitAlphaStats()
{
	m_monsterStatus.maxHp = 1000;
	m_monsterStatus.hp = 1000;
}
