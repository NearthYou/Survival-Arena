#include "pch.h"
#include "PlayerOutlineEffect.h"

PlayerOutlineEffect::PlayerOutlineEffect()
{
}

PlayerOutlineEffect::~PlayerOutlineEffect()
{
}

void PlayerOutlineEffect::Start()
{
	Super::Start();
}

void PlayerOutlineEffect::Update()
{
	Super::Update();

	if (GetGameObject()->GetType() == OBJECTTYPE::PLAYER) {
		if (m_isGlowing) {
			SetGlowEffect(true);
		}
	}
	else {
		if (m_isGlowing) {
			SetGlowEffect(false);
		}
	}
}

void PlayerOutlineEffect::SetGlowEffect(bool _enable)
{

}
