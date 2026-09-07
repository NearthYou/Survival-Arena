#pragma once
#include "IBasePanelUI.h"
class TimePanelUI :
    public IBasePanelUI
{
public:
    TimePanelUI();
    virtual ~TimePanelUI();

    virtual void Initialize();
    virtual void Update();
    virtual void SetVisible(bool visible);
    virtual void Cleanup();

    void UpdateTime();

protected:

    virtual void CreatePanels();

    virtual void RegisterUIObject(shared_ptr<GameObject> uiObject);

private:
    float m_lastFloatTime = 0.f;
    int m_lastTime = -1;
};

