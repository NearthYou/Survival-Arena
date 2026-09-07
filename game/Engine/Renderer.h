#pragma once
#include "Component.h"

class Renderer : public Component
{
    using Super = Component;
public:
    Renderer(ComponentType _componentType);
    virtual ~Renderer();

    void SetPass(uint8 _pass) { m_pass = _pass; }
    virtual void SetMaterial(shared_ptr<Material> _material) { m_material = _material; }
    shared_ptr<Material> GetMaterial() { return m_material; }
    bool Render(bool _isShadowTech);

protected:
    virtual void InnerRender(bool _isShadowTech);

protected:
    shared_ptr<Material> m_material;
    uint8 m_pass = 0;

public:
    void SetActive(bool active) { m_isActive = active; }
    bool IsActive() const { return m_isActive; }

protected:
    bool m_isActive = true;
};

