#include "pch.h"
#include "PlayerInterface.h"
#include "Player.h"
#include "BaseSkill.h"


float PlayerInterface::GetMaxSkillCooldown(int skillIndex) const
{
	return m_player.lock()->m_skills[skillIndex]->GetMaxCooldown();
}

float PlayerInterface::GetCurSkillCooldown(int skillIndex) const
{
	return m_player.lock()->m_skills[skillIndex]->GetCurrentCooldown();
}
