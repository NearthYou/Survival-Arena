#pragma once
#include "IBasePanelUI.h"

class Player;

class GameHUDPanelUI :
    public IBasePanelUI
{
public:
    GameHUDPanelUI(shared_ptr<Player> player, int selectedCharIdx);
    virtual ~GameHUDPanelUI();

    virtual void Initialize();
    virtual void Update();
    virtual void SetVisible(bool visible);
    virtual void Cleanup();


    void UpdateSkillCoolDown();
    void UpdateStatBar();
    void UpdatePlayerLevel();
    void UpdateSkillLevelBar(int skillIndex);

protected:
    virtual void CreatePanels();

    void CreateSkillIcons();
    void CreateStatBars();
    void CreateCharacterImage();

    virtual void RegisterUIObject(shared_ptr<GameObject> uiObject);

private:
    shared_ptr<Player> m_player;


    int m_selectedCharIdx;
};

