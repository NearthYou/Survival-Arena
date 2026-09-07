#include "pch.h"
#include "Wolf.h"
#if defined(DXA_GAME_PROBE)
#include "GameProbe.h"
#endif
#include "ItemBox.h"

#include "WolfAppearState.h"
#include "WolfDeathState.h"
#include "WolfDyingState.h"
#include "WolfRunState.h"
#include "WolfWaitState.h"
#include "WolfTraceState.h"
#include "WolfAttackState.h"

#include "WolfAnimAppearState.h"
#include "WolfAnimDeathState.h"
#include "WolfAnimDyingState.h"
#include "WolfAnimRunState.h"
#include "WolfAnimWaitState.h"
#include "WolfAnimTraceState.h"
#include "WolfAnimAttackState.h"

#include "WolfBaseAttack.h"
#include "WolfTrace.h"


#include "MonsterStateMachine.h"
#include "MonsterInterface.h"


#include "SkillObject.h"
#include "Player.h"

Wolf::Wolf(shared_ptr<Shader> _shader)
	: Super(_shader)
{
	SetName(L"Wolf");
	m_itembox = make_shared<ItemBox>();
}

Wolf::~Wolf()
{
}

void Wolf::Start()
{
	InitWolfModel();
	InitWolfAnimation();
	InitWolfMSM();
	InitWolfComponent();
	//InitWolfAI();
	
	InitWolfStats();
	UpdateState();

	Super::Start();
}

void Wolf::Update()
{
#if defined(DXA_GAME_PROBE)
    DXA_GAME_PROBE_FUNCTION(static_pointer_cast<Wolf>(shared_from_this()));
#endif

	Super::Update();
}

void Wolf::LateUpdate()
{
	Super::LateUpdate();
}

void Wolf::FixedUpdate()
{
	Super::FixedUpdate();
}

void Wolf::OnCollision(shared_ptr<GameObject> _other)
{
}

void Wolf::OnCollisionEnter(shared_ptr<GameObject> _other)
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
	//	//static_pointer_cast<WolfTraceState>(m_monsterStateMachine->GetState(MonsterStateType::Trace))->SetOtherObject(chaseTarget);
	//	//static_pointer_cast<WolfAttackState>(m_monsterStateMachine->GetState(MonsterStateType::Attack))->SetOtherObject(chaseTarget);
	//	
	//	GetComponent<WolfBaseAttack>()->SetTarget(chaseTarget);
	//	
	//	//m_monsterStateMachine->ChangeState(MonsterStateType::Trace);
	//	//m_animationStateMachine->ChangeState(AnimationStateType::Trace);
	//}

	// 피격 시 타겟 설정은 Monster::Damaged에서 처리하므로
	// 단순히 피격 플래그만 설정
	if (_other->GetType() == OBJECTTYPE::PLAYER ||
		(dynamic_pointer_cast<SkillObject>(_other) &&
			dynamic_pointer_cast<SkillObject>(_other)->GetOwner()->GetType() == OBJECTTYPE::PLAYER))
	{
		SetAttacked(true);
		cout << "몬스터가 공격받음" << endl;
	}
}

void Wolf::OnCollisionExit(shared_ptr<GameObject> _other)
{
	SetAttacked(false);
}


void Wolf::UpdateState()
{
}

void Wolf::InitWolfModel()
{
	m_model = make_shared<Model>();
	m_model->ReadModel(L"wolf/wolf_mesh");
	m_model->ReadMaterial(L"wolf/wolf_mesh");
}

void Wolf::InitWolfAnimation()
{
	m_model->ReadAnimation(L"Appear", L"wolf/wolf_appear_anim");
	m_model->ReadAnimation(L"AppearWait", L"wolf/wolf_appearwait_anim");
	m_model->ReadAnimation(L"Atk1", L"wolf/wolf_atk1_anim");
	m_model->ReadAnimation(L"Atk2", L"wolf/wolf_atk2_anim");
	m_model->ReadAnimation(L"Death", L"wolf/wolf_death_anim");
	m_model->ReadAnimation(L"Dying", L"wolf/wolf_dying_anim");
	m_model->ReadAnimation(L"Run", L"wolf/wolf_run_anim");
	//m_model->ReadAnimation(L"Skill", L"wolf/wolf_skill_anim");
	m_model->ReadAnimation(L"Wait", L"wolf/wolf_wait_anim");

	AddComponent(make_shared<ModelAnimator>(m_defaultShader));
	{
		GetModelAnimator()->SetModel(m_model);
		GetModelAnimator()->SetPass(2);
	}

	//FSM 추가. 
	m_animationStateMachine = make_shared<AnimationStateMachine>(AnimationStateType::Appear);
	AddComponent(m_animationStateMachine);
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Wait,			make_shared<WolfAnimWaitState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Appear,		make_shared<WolfAnimAppearState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Run,			make_shared<WolfAnimRunState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::BaseAttack, make_shared<WolfAnimAttackState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Death,		make_shared<WolfAnimDeathState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Dying,		make_shared<WolfAnimDyingState>());
	GetAnimationStateMachine()->RegisterState(AnimationStateType::Trace,		make_shared<WolfAnimTraceState>());

	auto animator = GetModelAnimator();

	// Wolf 등장 시퀀스. 
	vector<wstring> appearAnims = { L"Appear", L"AppearWait"};
	vector<float> appearAnimsDurations;
	appearAnimsDurations.push_back(animator->GetAnimationDuration(L"Appear")); 
	appearAnimsDurations.push_back(animator->GetAnimationDuration(L"AppearWait"));
	animator->CreateSequence(L"Wolf_Appear_Sequence", appearAnims, appearAnimsDurations, false);

	// Wolf 사망하는 시퀀스. 
	vector<wstring> deadAnims = { L"Death" };
	vector<float> deadAnimsDurations;
	deadAnimsDurations.push_back(animator->GetAnimationDuration(L"Death"));
	animator->CreateSequence(L"Wolf_death_Sequence", deadAnims, deadAnimsDurations, false);

	// Wolf 완전 죽어서 시체인 상태 시퀀스. 
	vector<wstring> dyingAnims = { L"Dying" };
	vector<float> dyingAnimsDurations;
	dyingAnimsDurations.push_back(animator->GetAnimationDuration(L"Dying"));
	animator->CreateSequence(L"Wolf_dying_Sequence", dyingAnims, dyingAnimsDurations, true);

	// Wolf 달리기
	vector<wstring> runAnims = { L"Run" };
	vector<float> runAnimsDurations;
	runAnimsDurations.push_back(animator->GetAnimationDuration(L"Run"));
	animator->CreateSequence(L"Wolf_Run_Sequence", runAnims, runAnimsDurations, true);


	// Wolf 공격모션 1
	vector<wstring> atk1Anims = { L"Atk1" };
	vector<float> atk1AnimsDurations;
	atk1AnimsDurations.push_back(animator->GetAnimationDuration(L"Atk1"));
	animator->CreateSequence(L"Wolf_Atk1_Sequence", atk1Anims, atk1AnimsDurations, false);

	// Wolf 공격모션 2
	vector<wstring> atk2Anims = { L"Atk2" };
	vector<float> atk2AnimsDurations;
	atk2AnimsDurations.push_back(animator->GetAnimationDuration(L"Atk2"));
	animator->CreateSequence(L"Wolf_Atk2_Sequence", atk2Anims, atk2AnimsDurations, false);

}

void Wolf::InitWolfComponent()
{
	m_collider = make_shared<SphereCollider>();
	m_collider->SetOffset(Vec3(0, 1, 0));
	m_collider->SetOffsetScale(Vec3(1.f, 1.f, 1.f));
	m_collider->SetVisible(false);

	//m_collider->SetVisible(false);
	m_rigidbody = make_shared<Rigidbody>();
	m_navAgent = make_shared<NavMeshAgent>();
	//m_itembox = make_shared<ItemBox>();

	//행동 스크립트? 컴포넌트?
	auto attackScript = make_shared<WolfBaseAttack>();
	attackScript->SetOwner(shared_from_this());
	AddComponent(attackScript);

	auto traceScript = make_shared<WolfTrace>();
	traceScript->SetOwner(shared_from_this());
	AddComponent(traceScript);

	AddComponent(m_collider);
	AddComponent(m_rigidbody);
	AddComponent(m_navAgent);
	AddComponent(m_itembox);	
}

void Wolf::InitWolfAI()
{
	/*auto sharedThis = dynamic_pointer_cast<Monster>(shared_from_this());

	auto appearAI = make_shared<WolfAppearAI>(sharedThis);
	auto attackAI = make_shared<WolfAttackAI>(sharedThis);
	auto deathAI = make_shared<WolfDeathAI>(sharedThis);
	auto idleAI = make_shared<WolfIdleAI>(sharedThis);

	m_AIMap[L"Appear"] = appearAI;
	m_AIMap[L"Attack"] = attackAI;
	m_AIMap[L"Death"] = deathAI;
	m_AIMap[L"Idle"] = idleAI;

	m_curAI = appearAI;
	m_curAI->Enter();*/
}

void Wolf::InitWolfMSM()
{
	m_monsterStateMachine = make_shared<MonsterStateMachine>();
	AddComponent(m_monsterStateMachine);


	auto self = static_pointer_cast<Monster>(shared_from_this());
	m_monsterInterface = make_shared<MonsterInterface>(self);

	m_monsterStateMachine->SetMonsterInterface(m_monsterInterface);

	m_monsterStateMachine->RegisterState(MonsterStateType::Wait, make_shared<WolfWaitState>());
	m_monsterStateMachine->RegisterState(MonsterStateType::Appear, make_shared<WolfAppearState>());
	m_monsterStateMachine->RegisterState(MonsterStateType::Run, make_shared<WolfRunState>());
	m_monsterStateMachine->RegisterState(MonsterStateType::Death, make_shared<WolfDeathState>(shared_from_this()));
	m_monsterStateMachine->RegisterState(MonsterStateType::Dying, make_shared<WolfDyingState>());
	m_monsterStateMachine->RegisterState(MonsterStateType::Trace, make_shared<WolfTraceState>(shared_from_this()));
	m_monsterStateMachine->RegisterState(MonsterStateType::Attack, make_shared<WolfAttackState>(shared_from_this()));
}

void Wolf::InitWolfStats()
{
	m_monsterStatus.maxHp = 200;
	m_monsterStatus.hp = 200;
}
