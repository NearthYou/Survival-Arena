#pragma once
#include "MonsterBehaviour.h"

class BiancaESkillCircle;
class Alpha;
class AlphaSkill : MonsterBehaviour
{
public:
	AlphaSkill(shared_ptr<GameObject> _alpha);
	virtual ~AlphaSkill();

public:
	virtual void Start() override;
	virtual void Update() override;
	void Play(shared_ptr<GameObject> _target);

private:
	shared_ptr<BiancaESkillCircle> m_circleObjects[5];
	shared_ptr<GameObject> m_alpha;

	float m_skillElapsedTime = 0.f;
	float m_skillDuration = 2.25f;

	float m_skillEffectDuration = 1.f;
	float m_skillEffectElapsedTime = 0.f;

	float m_skilRange = 6.5f;
	bool m_isActive = false;
	bool m_boozer = false;
};

