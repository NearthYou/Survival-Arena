#include "pch.h"
#include "BiancaTest.h"
#include "Model.h"
#include "GameObject.h"
#include "Transform.h"
#include "ModelAnimator.h"

void BiancaTest::Init()
{
}

void BiancaTest::Update()
{
	float dt = TIME->GetDeltaTime();
	Vec3 pos = GetTransform()->GetPosition();
	BiancaState curState;
	if (INPUT->GetButton(KEY_TYPE::W) || INPUT->GetButton(KEY_TYPE::A) || INPUT->GetButton(KEY_TYPE::S) || INPUT->GetButton(KEY_TYPE::D)) {
		Vec3 currentRotation = GetTransform()->GetRotation();
		float targetYaw = 0.0f;
		Vec3 dir = Vec3::Zero;
		if (INPUT->GetButton(KEY_TYPE::W)) {
			dir += Vec3(0, 0, 1);
			targetYaw = 180.f;
		}

		if (INPUT->GetButton(KEY_TYPE::S)) {
			dir -= Vec3(0, 0, 1);
			targetYaw = 0.f;
		}

		if (INPUT->GetButton(KEY_TYPE::A)) {
			dir -= Vec3(1, 0, 0);
			targetYaw = 90.f;
		}

		if (INPUT->GetButton(KEY_TYPE::D)) {
			dir += Vec3(1, 0, 0);
			targetYaw = 270.f;
		}
		dir.Normalize();

		Vec3 rotation = Vec3(0.0f, targetYaw, 0.0f);
		GetTransform()->SetLocalRotation(rotation);
		curState = BiancaState::RUN;
		pos += dir * m_speed * dt;


	}
	else if (INPUT->GetButton(KEY_TYPE::LBUTTON)) {
		curState = BiancaState::ATK;
	}
	else if (INPUT->GetButton(KEY_TYPE::RBUTTON)) {
		curState = BiancaState::DANCE;
	}
	else {
		curState = BiancaState::WAIT;
	}
	GetTransform()->SetPosition(pos);
	ChangeState(curState);
}

void BiancaTest::ChangeState(BiancaState _state)
{
	if (m_state == _state) return;
	auto animator = GetGameObject()->GetModelAnimator();
	if (_state == BiancaState::WAIT) {
		animator->SetNextAnimation(0, 0.2f);
	}
	else if (_state == BiancaState::RUN) {
		animator->SetNextAnimation(1, 0.2f);
	}
	else if (_state == BiancaState::DANCE) {
		animator->SetAnimation(6, true);
	}
	else if (_state == BiancaState::ATK) {
		animator->SetAnimation(2, true);
	}
	m_state = _state;
}
