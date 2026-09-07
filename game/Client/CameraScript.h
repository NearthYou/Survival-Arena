#pragma once
#include "MonoBehaviour.h"
class CameraScript : public MonoBehaviour
{
public:

private:
	virtual void Init() override;
	virtual void Update() override;


private:
	float m_speed = 100.f;
};