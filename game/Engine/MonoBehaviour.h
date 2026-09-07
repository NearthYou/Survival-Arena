#pragma once
#include "Component.h"
class MonoBehaviour :
    public Component
{
    using Super = Component;

public:
    MonoBehaviour();
    virtual ~MonoBehaviour();

    virtual void Init() override;
    virtual void Update() override;

    virtual void SetActive(bool _active) { m_active = _active; }
    virtual bool IsActive() { return m_active; }

private:
    bool m_active = true;
};

