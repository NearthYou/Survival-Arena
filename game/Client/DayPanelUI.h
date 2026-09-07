#pragma once
#include "IBasePanelUI.h"
class DayPanelUI :
    public IBasePanelUI
{
public:
    DayPanelUI();
    virtual ~DayPanelUI();

    virtual void Initialize();
    virtual void Update();
    virtual void SetVisible(bool visible);
    virtual void Cleanup();

protected:

    virtual void CreatePanels();

    virtual void RegisterUIObject(shared_ptr<GameObject> uiObject);

};

