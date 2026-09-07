#pragma once

class IBasePanelUI
{
public:
    IBasePanelUI() = default;
    virtual ~IBasePanelUI() = default;

    virtual void Initialize() = 0;
    virtual void Update() = 0;
    virtual void SetVisible(bool visible) = 0;
    virtual void Cleanup() = 0;
    void OffsetForViewport(const Vec2& offset);

protected:
    virtual void CreatePanels() = 0;

    virtual void RegisterUIObject(shared_ptr<GameObject> uiObject) = 0;

    shared_ptr<GameObject> m_panel;
    bool m_isVisible = true;
};

