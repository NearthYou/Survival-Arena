#include "pch.h"
#include "NickyWSkill.h"

#include "Player.h"
#include "PlayerStateMachine.h"
#include "AnimationStateMachine.h"

NickyWSkill::NickyWSkill(shared_ptr<Player> _player)
	: Super(_player, 1)
	, m_player(_player)
{
	m_skillCooldown = 5.f;
    ConfigureProgression(25);
	m_skillImage = RESOURCES->GetOrAddTexture(L"NickyW", L"..\\Resources\\Textures\\UI\\SkillIcon\\SkillIcon_1033300.png");
}

NickyWSkill::~NickyWSkill()
{

}

void NickyWSkill::PlaySkill()
{
    m_skillTimer = 0.f;
	SOUND->PlaySound(L"Nicky/Nicky_skill02_Gaurd.wav", 20, 0.5f);
}

void NickyWSkill::Update()
{
	UpdateSkillCoolDown();

	if (m_player->GetPlayerStateMachine()->IsInState(PlayerStateType::Skill_2))
	{
		m_skillTimer += DT;
		if (m_skillTimer >= m_skillDuration)
		{
			m_skillTimer = 0.f;
			SkillEnd();
		}
	}
}
