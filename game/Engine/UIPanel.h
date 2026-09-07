#pragma once
#include "Component.h"
#include "Text.h"
#include "D2DText.h"

class Material;
class Mesh;
class Texture;
class Button;
class Text;
class ImageUI;
class SliderUI;

class UIPanel : public Component
{
    using Super = Component;

public:
    UIPanel();
    virtual ~UIPanel();

    virtual void Init() override;
    virtual void Update() override;

    // 소멸 관련 메서드
    virtual void OnDestroy() override;
    void ClearChildReferences() {
        // weak_ptr은 순환 참조를 만들지 않음
        m_childElements.clear();
        m_namedElements.clear();
    }

    // 패널 설정 함수들
    void SetPosition(const Vec2& position);
    void SetSize(const Vec2& size);
    void SetBackgroundColor(const Vec4& color);
    void SetBackgroundMaterial(shared_ptr<Material> material);
    void SetVisible(bool visible);

    // UI 요소 추가 함수들
    shared_ptr<UIPanel> AddPanel(Vec2 localPos, Vec2 size, shared_ptr<Material> material, const wstring& name = L"ChildPanel");
    shared_ptr<Button> AddButton(Vec2 localPos, Vec2 size, shared_ptr<Material> material, const wstring& name = L"Button");
    shared_ptr<Text> AddText(Vec2 localPos, const wstring& text, float fontSize = 16.0f,
        Vec4 color = Vec4(1, 1, 1, 1), float alpha = 1.0f,
        Vec4 outlineColor = Vec4(0, 0, 0, 1), float outlineWidth = 1.0f,
        const wstring& name = L"Text",
        TextAlignment alignment = TextAlignment::Left);
    shared_ptr<ImageUI> AddImageUI(Vec2 localPos, const wstring& name = L"ImageUI");
    shared_ptr<SliderUI> AddSliderUI(Vec2 localPos, Vec2 size, shared_ptr<Material> trackMaterial, shared_ptr<Material> fillMaterial, shared_ptr<Material> handleMaterial, float minValue = 0.0f, float maxValue = 1.0f, const wstring& name = L"Slider");
    shared_ptr<D2DText> AddD2DText(Vec2 localPos, const wstring& text, float fontSize = 16.0f,
        Vec4 color = Vec4(1, 1, 1, 1), float alpha = 1.0f,
        Vec4 outlineColor = Vec4(0, 0, 0, 1), float outlineWidth = 1.0f,
        const wstring& name = L"D2DText",
        TextAlignment alignment = TextAlignment::Left);
  
    // UI 요소 관리
    void RemoveUIElement(const wstring& name);
    shared_ptr<UIPanel> GetChildUIPanel(const wstring& name);
    shared_ptr<Button> GetButton(const wstring& name);
    shared_ptr<Text> GetText(const wstring& name);
    shared_ptr<ImageUI> GetImageUI(const wstring& name);
    shared_ptr<SliderUI> GetSliderUI(const wstring& name);
    shared_ptr<D2DText> GetD2DText(const wstring& name);  // D2DText getter 추가
    const vector<weak_ptr<GameObject>>& GetChildElements() const { return m_childElements; }
    void RemoveUIElementSafely(const wstring& name);


    // Getter 함수들
    const Vec2& GetPosition() const { return m_position; }
    const Vec2& GetSize() const { return m_size; }
    bool IsVisible() const { return m_visible; }
    bool Picked(POINT _screenPos);

    // 패널 생성 함수
    void Create(Vec2 screenPos, Vec2 size, Vec4 diffuseInfo, shared_ptr<Material> backgroundMaterial = nullptr);

    //유틸
    Vec2 LocalToWorldPosition(const Vec2& localPos);

    //드래깅
    void SetDraggable(bool draggable) { m_isDraggable = draggable; }
    bool IsDraggable() const { return m_isDraggable; }
    bool IsDragging() const { return m_isDragging; }

    void StartDrag(Vec2 mousePos);
    void UpdateDrag(Vec2 mousePos);
    void EndDrag();

    void HandleDragInput();

    void SetIsChildPanel(bool _isChildPanel) { m_isChildPanel = _isChildPanel; }
    bool IsChildPanel() { return m_isChildPanel; }

    void SetLocalPosition(const Vec2& localPos) { m_localPosition = localPos; }
    const Vec2& GetLocalPosition() const { return m_localPosition; }
 
private:
    void CreatePanelBackground();
    void UpdateChildPositions();
    void UpdatePosition(const Vec2& parentWorldPos);

private:
    Vec2 m_localPosition = Vec2::Zero;
    Vec2 m_position = Vec2(0.0f, 0.0f);
    Vec2 m_size = Vec2(200.0f, 150.0f);
    RECT m_rect;
    Vec4 m_backgroundColor = Vec4(1.f);
    bool m_visible = true;

    bool m_isChildPanel = false;

    // 배경 렌더링용
    shared_ptr<Texture> m_backgroundTexture;
    shared_ptr<Material> m_backgroundMaterial;
    shared_ptr<Mesh> m_backgroundMesh;

    // 자식 UI 요소들
    vector<weak_ptr<GameObject>> m_childElements;
    map<wstring, weak_ptr<GameObject>> m_namedElements;

    bool m_isDestroying = false;  // 소멸 중 플래그 추가

    // 드래그 관련 변수들
    bool m_isDraggable = false;          // 드래그 가능 여부
    bool m_isDragging = false;          // 현재 드래그 중인지
    Vec2 m_dragStartPos;                // 드래그 시작 위치 (화면 좌표)
    Vec2 m_panelStartPos;               // 드래그 시작 시 패널 위치
};