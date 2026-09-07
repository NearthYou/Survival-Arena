#pragma once
#include "MonoBehaviour.h"
class PlayerOutlineEffect :
    public MonoBehaviour
{
    using Super = MonoBehaviour;

public:
    PlayerOutlineEffect();
    virtual ~PlayerOutlineEffect();

    virtual void Start() override;
    virtual void Update() override;

private:
    void SetGlowEffect(bool _enable);

private:

    OutlineDesc m_outlineDesc = {};

    Color m_outlineColor = Color(1.f, 1.f, 0.f, 1.f);
    float m_glowIntensity = 2.f;
    bool m_isGlowing = true;


};

