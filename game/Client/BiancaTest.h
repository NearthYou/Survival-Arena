#pragma once

#include "MonoBehaviour.h"
class Model;

enum class BiancaState {
	WAIT,
	RUN,
	REST,
	ATK,
	BOXOPEN,
	COLLECT,
	CRAFT,
	DANCE, 
};
class BiancaTest :
    public MonoBehaviour
{
public:

private:
	virtual void Init() override;
	virtual void Update() override;

	void ChangeState(BiancaState _state);

private:
	BiancaState m_state;
	float m_speed = 10.f;
};

