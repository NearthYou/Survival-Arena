#pragma once
#include "BaseSkill.h"
class BiancaWSkill :
    public BaseSkill
{
    using Super = BaseSkill;
public:
    BiancaWSkill(shared_ptr<Player> _player);
    virtual ~BiancaWSkill();

public:
    virtual void PlaySkill() override;
    virtual void Update() override;

private:
    bool m_isPlaying = false;
    float m_repeatKey = 0.f;
    float m_duration = 3.f;
    float m_elapsedTime = 0.f;
    shared_ptr<GameObject> m_coffin;
    shared_ptr<Shader> m_shader;
    shared_ptr<Player> m_player;

private:

    float m_skillDuration = (101.f / 25.f) / 2.f;
    float m_skillTimer = 0.f;


private:
    const wstring m_soundStart = L"Bianca/Bianca_Skill02_Active.wav";
    const wstring m_soundEnd = L"Bianca/Bianca_Skill02_End.wav";
};

