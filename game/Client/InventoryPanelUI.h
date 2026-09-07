#pragma once

#include "IBasePanelUI.h"

class ItemSlot;
class Item;

class InventoryPanelUI :
    public IBasePanelUI
{
public:
    InventoryPanelUI();
    ~InventoryPanelUI();

    virtual void Initialize();
    virtual void Update();
    virtual void SetVisible(bool visible);
    virtual void Cleanup();

public:
    void CreateInventorySlots();
    
 

public:
    vector<shared_ptr<ItemSlot>>& GetInventorySlots() { return m_inventorySlots; }
    void CreateBaseSlots();
protected:
    virtual void CreatePanels();

    virtual void RegisterUIObject(shared_ptr<GameObject> uiObject);


private:
    shared_ptr<GameObject> m_basePanel;
    vector<shared_ptr<ItemSlot>> m_inventorySlots;
 
};


