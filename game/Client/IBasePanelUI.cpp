#include "pch.h"
#include "IBasePanelUI.h"

void IBasePanelUI::OffsetForViewport(const Vec2& offset)
{
    if (m_panel && m_panel->GetUIPanel())
        m_panel->GetUIPanel()->SetPosition(m_panel->GetUIPanel()->GetPosition() + offset);
}
