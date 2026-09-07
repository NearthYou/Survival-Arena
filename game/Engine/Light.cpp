#include "pch.h"
#include "Light.h"
#include "Camera.h"

Matrix Light::s_MatView = Matrix::Identity;
Matrix Light::s_MatProjection = Matrix::Identity;
Matrix Light::s_ShadowTransform = Matrix::Identity;

Light::Light() : Component(ComponentType::Light)
{
}

Light::~Light()
{
   
}

void Light::Update()
{
	//RENDER->PushLightData(m_desc);
}

void Light::SetVPMatrix(Camera* _camera, float _backDist, Matrix _matProjection)
{
    Vec3 camPos = _camera->GetTransform()->GetPosition();

    //  라이트 방향을 정규화된 Direction에서 가져오기
    Vec3 lightDir = Vec3(m_desc.direction.x, m_desc.direction.y, m_desc.direction.z);
    lightDir.Normalize();

    //  라이트 위치를 더 명확하게 계산
    Vec3 eyePosition = camPos + (-lightDir * _backDist);
    Vec3 focusPosition = camPos; // 카메라 위치를 바라보도록
    Vec3 upDirection = Vec3::Up;

    //  라이트가 거의 수직일 때 Up 벡터 조정
    if (abs(lightDir.y) > 0.95f) {
        upDirection = Vec3::Right;
    }

    s_MatView = ::XMMatrixLookAtLH(eyePosition, focusPosition, upDirection);
    s_MatProjection = _matProjection;

    Matrix T(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f);

    s_ShadowTransform = s_MatView * s_MatProjection * T;
}
