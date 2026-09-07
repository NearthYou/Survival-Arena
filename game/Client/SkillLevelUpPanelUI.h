#pragma once
#include "IBasePanelUI.h"

class Player;
class GameHUDPanelUI;

class SkillLevelUpPanelUI :
    public IBasePanelUI
{
public:
    SkillLevelUpPanelUI(shared_ptr<Player> player);
    virtual ~SkillLevelUpPanelUI();

    virtual void Initialize();
    virtual void Update();
    virtual void SetVisible(bool visible);
    virtual void Cleanup();

    void SetGameHUDPanelUI(weak_ptr<GameHUDPanelUI> _gameHUD) { m_gameHUD = _gameHUD; }

    void UpdateSkillLevelPanel();

protected:

    virtual void CreatePanels();

    virtual void RegisterUIObject(shared_ptr<GameObject> uiObject);

private:
    shared_ptr<Player> m_player;
    weak_ptr<GameHUDPanelUI> m_gameHUD;
};

