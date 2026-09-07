#pragma once
#include "Button.h"
#include "GameObject.h"
#include "Material.h"
#include "MeshRenderer.h"
#include "Shader.h"
#include "UIPanel.h"

namespace ArenaUi
{
inline Vec4 Ink() { return Vec4(0.075f, 0.105f, 0.12f, 1.f); }
inline Vec4 Panel() { return Vec4(0.14f, 0.19f, 0.21f, 1.f); }
inline Vec4 Paper() { return Vec4(0.86f, 0.88f, 0.85f, 1.f); }
inline Vec4 Copper() { return Vec4(0.77f, 0.58f, 0.32f, 1.f); }
inline Vec4 Muted() { return Vec4(0.49f, 0.61f, 0.61f, 1.f); }

inline shared_ptr<Material> Solid(const Vec4& color)
{
    auto material = make_shared<Material>();
    material->SetShader(make_shared<Shader>(L"ImageShader.fx"));
    material->SetRenderingMode(RenderingMode::Forward);
    material->SetRenderQueue(RenderQueue::Transparent);
    material->SetTransparent(true);
    material->GetMaterialDesc().diffuse = color;
    return material;
}

inline shared_ptr<GameObject> Canvas(float width, float height, const wstring& name)
{
    auto object = make_shared<GameObject>();
    object->SetName(name);
    object->AddComponent(make_shared<UIPanel>());
    object->GetUIPanel()->Create(Vec2(width / 2, height / 2), Vec2(width, height), Ink(), nullptr);
    object->SetLayerIndex(LAYER_UI);
    const auto position = object->GetTransform()->GetPosition();
    object->GetTransform()->SetPosition(Vec3(position.x, position.y, 0.9f));
    CURSCENE->AddUIObject(object, true);
    CURSCENE->RegisterUIParent(object);
    return object;
}

inline shared_ptr<UIPanel> Block(shared_ptr<UIPanel> parent, Vec2 position, Vec2 size,
    const Vec4& color, const wstring& name)
{
    auto block = parent->AddPanel(position, size, nullptr, name);
    block->SetBackgroundColor(color);
    return block;
}

inline shared_ptr<Button> Action(shared_ptr<UIPanel> parent, Vec2 position, Vec2 size,
    const wstring& caption, const wstring& name, const Vec4& color, float fontSize,
    std::function<void()> click)
{
    auto normal = Solid(color);
    auto hover = Solid(Vec4((std::min)(1.f, color.x + 0.1f), (std::min)(1.f, color.y + 0.1f),
        (std::min)(1.f, color.z + 0.1f), color.w));
    auto button = parent->AddButton(position, size, normal, name);
    button->SetNormalMaterial(normal);
    button->SetHoveredMaterial(hover);
    button->SetPressedMaterial(hover);
    button->GetGameObject()->GetMeshRenderer()->SetPass(2);
    button->OnClick += std::move(click);
    parent->AddText(position, caption, fontSize, Paper(), 1.f, Vec4(0.f), 0.f,
        name + L"Caption", TextAlignment::Center);
    return button;
}
}
