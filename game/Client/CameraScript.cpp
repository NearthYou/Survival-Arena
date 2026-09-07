#include "pch.h"
#include "CameraScript.h"
#include "Transform.h"
#include "GameObject.h"

void CameraScript::Init()
{
}

void CameraScript::Update()
{
	float dt = TIME->GetDeltaTime();

	Vec3 pos = GetTransform()->GetPosition();

	if (INPUT->GetButton(KEY_TYPE::T)) {
		pos += GetTransform()->GetLook() * m_speed * dt;
	}
	else if (INPUT->GetButton(KEY_TYPE::G)) {
		pos -= GetTransform()->GetLook() * m_speed * dt;
	}
	else if (INPUT->GetButton(KEY_TYPE::F)) {
		pos -= GetTransform()->GetRight() * m_speed * dt;
	}
	else if (INPUT->GetButton(KEY_TYPE::H)) {
		pos += GetTransform()->GetRight() * m_speed * dt;
	}
	GetTransform()->SetPosition(pos);

	Quaternion m_localQuaternion = Quaternion::Identity;
	if (INPUT->GetButton(KEY_TYPE::KEY_1))
	{
		Vec3 rotation = GetTransform()->GetLocalRotation();
		rotation.x += dt * 30.f;
		GetTransform()->SetLocalRotation(rotation);

		Quaternion rotationX = Quaternion::CreateFromAxisAngle(Vec3(1, 0, 0), XMConvertToRadians(dt * 30.f));
		m_localQuaternion = m_localQuaternion * rotationX;  // 순수한 X축 회전만 적용
	}

	if (INPUT->GetButton(KEY_TYPE::KEY_2))
	{
		Vec3 rotation = GetTransform()->GetLocalRotation();
		rotation.x -= dt * 30.f;
		GetTransform()->SetLocalRotation(rotation);
	}

	if (INPUT->GetButton(KEY_TYPE::KEY_3))
	{
		Vec3 rotation = GetTransform()->GetLocalRotation();
		rotation.y += dt * 30.f;
		GetTransform()->SetLocalRotation(rotation);
	}

	if (INPUT->GetButton(KEY_TYPE::KEY_4))
	{
		Vec3 rotation = GetTransform()->GetLocalRotation();
		rotation.y -= dt * 30.f;
		GetTransform()->SetLocalRotation(rotation);
	}

	Matrix matRotation = Matrix::CreateFromQuaternion(m_localQuaternion);

	Vec3 tpos = GetTransform()->GetPosition();
	Vec3 trot = GetTransform()->GetRotation();

	//cout << "Pos : " << tpos.x << " " << tpos.y << " " << tpos.z << "\n";
	//cout << "Rot : " << trot.x << " " << trot.y << " " << trot.z << "\n";
}
