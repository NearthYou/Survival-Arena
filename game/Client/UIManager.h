#pragma once

#include "Component.h"
#include "InventoryManager.h"

class Player;

class UIManager :
    public Component
{
    using Super = Component;
public:
    UIManager(shared_ptr<Player> player, int selectedCharIdx);
    virtual ~UIManager();

    //virtual void Start() override;
    virtual void Update() override;

    // UI 패널 관리
    void InitializeUI();
    void UpdateUI();
    void SetPlayer(shared_ptr<Player> player) { m_player = player; }

    void CreateInventoryManager();

 
    shared_ptr<class GameHUDPanelUI> GetGameHUD() { return m_gameHUD; }
    shared_ptr<class SkillLevelUpPanelUI> GetSkillLevelUpUI() { return m_skillLevelUp; }
    shared_ptr<class PlayerStatusPanelUI> GetStatusUI() { return m_playerStatus; }

    shared_ptr<class TimePanelUI> GetTimeUI() { return m_time; }
    shared_ptr<class DayPanelUI> GetDayUI() { return m_day; }
    shared_ptr<class EquipmentPanelUI> GetEquipmentUI() { return m_equipment; }
    shared_ptr<class InventoryPanelUI> GetInventoryUI() { return m_inventory; }
    shared_ptr<class CraftListPanelUI> GetCraftListUI() { return m_craftList; }
    shared_ptr<class CraftGagePanelUI> GetCraftGageUI() { return m_craftGage; }

private:
    shared_ptr<Player> m_player;

    // UI 매니저들
    shared_ptr<class GameHUDPanelUI>        m_gameHUD;
    shared_ptr<class SkillLevelUpPanelUI>   m_skillLevelUp;
    shared_ptr<class PlayerStatusPanelUI>   m_playerStatus;
    shared_ptr<class TimePanelUI>           m_time;
    shared_ptr<class DayPanelUI>            m_day;
    shared_ptr<class EquipmentPanelUI>      m_equipment;
    shared_ptr<class InventoryPanelUI>      m_inventory;
    shared_ptr<class CraftListPanelUI>      m_craftList;
    shared_ptr<class CraftGagePanelUI>      m_craftGage;

    shared_ptr<InventoryManager> m_inventoryManager;


    /*shared_ptr<class PlayerStatusUI> m_playerStatusUI;
    shared_ptr<class InventoryUI> m_inventoryUI;
    shared_ptr<class SkillUI> m_skillUI;
    
    shared_ptr<class ItemBoxUI> m_itemBoxUI;*/

    bool m_uiInitialized = false;
};

