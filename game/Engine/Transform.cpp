#include "pch.h"
#include "Transform.h"
#include "SimpleMath.h"

Transform::Transform() : Super(ComponentType::Transform)
{
}

Transform::~Transform()
{
}

Vec3 Transform::ToEulerAngles(Quaternion q)
{
	Vec3 angles;

	double test = q.x * q.y + q.z * q.w;

	// 특이점 처리 (짐벌락)
	if (test > 0.499) { // 북극 특이점
		angles.y = XMConvertToDegrees(2 * atan2(q.x, q.w));
		angles.x = XMConvertToDegrees(XM_PI / 2);
		angles.z = 0;
		return angles;
	}
	if (test < -0.499) { // 남극 특이점
		angles.y = XMConvertToDegrees(-2 * atan2(q.x, q.w));
		angles.x = XMConvertToDegrees(-XM_PI / 2);
		angles.z = 0;
		return angles;
	}

	// 일반적인 경우
	double sqx = q.x * q.x;
	double sqy = q.y * q.y;
	double sqz = q.z * q.z;

	// Y-X-Z 순서에 맞는 변환 공식
	angles.y = XMConvertToDegrees(atan2(2 * q.y * q.w - 2 * q.x * q.z, 1 - 2 * sqy - 2 * sqz));
	angles.x = XMConvertToDegrees(asin(2 * test));
	angles.z = XMConvertToDegrees(atan2(2 * q.x * q.w - 2 * q.y * q.z, 1 - 2 * sqx - 2 * sqz));

	return angles;
}

Vec3 Transform::NormalizeAngles(const Vec3& _angles)
{
	Vec3 result = _angles;

	// -180 ~ 180 범위로 정규화
	while (result.x > 180.0f) result.x -= 360.0f;
	while (result.x < -180.0f) result.x += 360.0f;

	while (result.y > 180.0f) result.y -= 360.0f;
	while (result.y < -180.0f) result.y += 360.0f;

	while (result.z > 180.0f) result.z -= 360.0f;
	while (result.z < -180.0f) result.z += 360.0f;

	return result;
}

void Transform::Init()
{
}

void Transform::Update()
{

}

void Transform::UpdateTransform()
{

	//Scale Rotation Translation

	Matrix matScale = Matrix::CreateScale(m_localScale);
	Matrix matRotation = Matrix::CreateFromQuaternion(m_localQuaternion);
	//Matrix matRotation = Matrix::CreateRotationY(XMConvertToRadians(m_localRotation.y));
	//matRotation *= Matrix::CreateRotationX(XMConvertToRadians(m_localRotation.x));
	//matRotation *= Matrix::CreateRotationZ(XMConvertToRadians(m_localRotation.z));
	Matrix matTranslation = Matrix::CreateTranslation(m_localPosition);

	m_matLocal = matScale * matRotation * matTranslation;

	//부모가 없으면 local행렬이 world와 같음. 

	//부모가 있을 때는 부모 좌표계를 기준으로 변환. 
	if (HasParent()) {
		m_matWorld = m_matLocal * m_parent->GetWorldMatrix();
	}
	else {
		m_matWorld = m_matLocal;
	}

	//여기서 right, up, look 벡터는 무엇인가??
	Quaternion quat;
	m_matWorld.Decompose(m_WorldScale, quat, m_WorldPosition);

	m_WorldRotation = ToEulerAngles(quat);



	//Coord와 Normal방식. 
	//방향만 바꾸고 싶다. 면, (1,0, 0, 1) <- 4번째가 0이냐 1이냐 차이. 

	//Children들 Transform변경
	for (const shared_ptr<Transform>& child : m_children) {
		child->UpdateTransform();
	}
}

void Transform::SetScale(const Vec3& _Scale)
{
	if (HasParent()) {
		Vec3 parentScale = m_parent->GetScale();
		Vec3 scale = m_WorldScale;

		scale.x /= parentScale.x;
		scale.y /= parentScale.y;
		scale.z /= parentScale.z;
		SetLocalScale(scale);
	}
	else {
		SetLocalScale(_Scale);
	}
}

void Transform::SetRotation(const Vec3& _Rotation)
{
	if (HasParent()) {
		// 목표 world rotation을 쿼터니언으로 변환
		Quaternion worldQuat = Quaternion::CreateFromYawPitchRoll(
			XMConvertToRadians(_Rotation.y),
			XMConvertToRadians(_Rotation.x),
			XMConvertToRadians(_Rotation.z)
		);

		// 부모 rotation도 쿼터니언으로
		Vec3 parentRot = m_parent->GetRotation();
		Quaternion parentQuat = Quaternion::CreateFromYawPitchRoll(
			XMConvertToRadians(parentRot.y),
			XMConvertToRadians(parentRot.x),
			XMConvertToRadians(parentRot.z)
		);

		// Local = World * Parent^-1
		XMVECTOR worldQuatVec = XMLoadFloat4(&worldQuat);
		XMVECTOR parentQuatVec = XMLoadFloat4(&parentQuat);
		XMVECTOR parentInverseVec = XMQuaternionInverse(parentQuatVec);  // 이렇게!
		XMVECTOR localQuatVec = XMQuaternionMultiply(worldQuatVec, parentInverseVec);

		Quaternion localQuat;
		XMStoreFloat4(&localQuat, localQuatVec);

		// 쿼터니언을 Vec3로 변환하여 설정
		Vec3 localRotation = ToEulerAngles(localQuat);
		SetLocalRotation(localRotation);
	}
	else {
		SetLocalRotation(_Rotation);
	}
}

void Transform::SetPosition(const Vec3& _Position)
{
	if (HasParent()) {
		//World -> Parent좌표계로 변경. 
		Matrix worldToParentLocalMatrix = m_parent->GetWorldMatrix().Invert();


		Vec3 position = Vec3::Transform(_Position, worldToParentLocalMatrix);
		SetLocalPosition(position);
	}
	else {
		SetLocalPosition(_Position);
	}
}


