#pragma once
#include "IBasePanelUI.h"

class ItemSlot;

class EquipmentPanelUI :
    public IBasePanelUI
{
public:
    EquipmentPanelUI();
    virtual ~EquipmentPanelUI();

    virtual void Initialize();
    virtual void Update();
    virtual void SetVisible(bool visible);
    virtual void Cleanup();


    void CreateEquipmentSlots();

public:
    vector<shared_ptr<ItemSlot>>& GetEquipmentSlots() { return m_equipmentSlots; }

protected:

    virtual void CreatePanels();

    virtual void RegisterUIObject(shared_ptr<GameObject> uiObject);


private:
    vector<shared_ptr<ItemSlot>> m_equipmentSlots;
};

