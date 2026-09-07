#pragma once
#include "IBasePanelUI.h"

class Player;

class PlayerStatusPanelUI :
    public IBasePanelUI
{
public:
    PlayerStatusPanelUI(shared_ptr<Player> player);
    virtual ~PlayerStatusPanelUI();

    virtual void Initialize();
    virtual void Update();
    virtual void SetVisible(bool visible);
    virtual void Cleanup();

    void UpdatePlayerStatus();

public:
    Vec4 ColorNormalize(Vec4 input);


protected:
    virtual void CreatePanels();

    virtual void RegisterUIObject(shared_ptr<GameObject> uiObject);

private:
    shared_ptr<Player> m_player;
};

