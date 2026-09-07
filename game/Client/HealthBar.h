#pragma once
#include "MonoBehaviour.h"


class UIPanel;
class ImageUI;

class HealthBar :
    public MonoBehaviour
{
    using Super = MonoBehaviour;

public:
    HealthBar();
    virtual ~HealthBar();

    virtual void Start() override;
    virtual void Update() override;

    void Create(Vec3 _offset = Vec3(0.f, 2.5f, 0.f));

    void UpdateHealthBar(int _curHP, int _maxHP, int _curMP, int _maxMP);

    void SetVisible(bool _visible);

private:
    void UpdateHealthBarPosition();
    void UpdateHealthBarSize(float _healthRatio);

private:
    friend class Player;

    float healthUpdateTime = 0.f;
    shared_ptr<UIPanel> m_healthBarPanel;
    shared_ptr<ImageUI> m_healthBarUI;
    shared_ptr<ImageUI> m_manaBarUI;
    
    Vec3 m_offset;
    Vec2 m_barSize;
    Vec2 m_manaBarSize;

    int m_lastCurHP = -1;
    int m_lastMaxHP = -1;

    int m_lastCurMP = -1;
    int m_lastMaxMP = -1;

    Vec3 m_lastTargetPos;
};

