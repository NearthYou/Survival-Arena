#pragma once
#include "MonoBehaviour.h"
class Cursor :
    public MonoBehaviour
{
    using Super = MonoBehaviour;

public:
    Cursor();
    virtual ~Cursor();

    virtual void Start() override;
    virtual void Update() override;

    void SetVisible(bool _visible);

private:
    void UpdateCursorPosition();

private:
    shared_ptr<UIPanel> m_cursorPanel;
    shared_ptr<ImageUI> m_cursorImageUI;

    Vec2 m_cursorSize = Vec2(64, 64);
    bool m_isVisible = true;
};

