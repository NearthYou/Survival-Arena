#pragma once
#include "IBasePanelUI.h"

#include "Item.h"
class Player;

class CraftGagePanelUI :
    public IBasePanelUI
{
public:
    CraftGagePanelUI(shared_ptr<Player> player);
    virtual ~CraftGagePanelUI();

    virtual void Initialize();
    virtual void Update();
    virtual void SetVisible(bool visible);
    virtual void Cleanup();


public:
    void UpdateGageBar();
    void SetItem(shared_ptr<Item> item) { m_createdItem = item; }

protected:
    virtual void CreatePanels();

    virtual void RegisterUIObject(shared_ptr<GameObject> uiObject);

private:
    float GetCraftTimeByGrade(ITEMGRADE grade);
    

private:
    shared_ptr<Player> m_player;
  
    shared_ptr<Item> m_createdItem;
    float m_duration = 0.f;
};

