#pragma once
#include "Component.h"
#include "Delegate.h"

enum class ButtonState
{
    Normal,
    Hovered,
    Pressed,
    Disabled
};

class Button : public Component
{
    using Super = Component;

    // 입력 상태 구조체 (최적화)
    struct MouseInputState
    {
        bool mouseInside;
        bool leftPressed;
        bool leftDown;
        bool leftUp;
        bool rightPressed;
        bool rightDown;
        bool rightUp;
    };

public:
    Button();
    virtual ~Button();

    virtual void Update() override;

    // 기본 인터페이스
    bool Picked(POINT screenPos) const;
    void Create(Vec2 localPos, Vec2 size, shared_ptr<Material> material, uint32 pass = 0);

    // 이벤트 (레거시 지원)
    void AddOnClickedEvent(std::function<void(void)> func);
    void InvokeOnClicked();
    void AddOnRightClickedEvent(std::function<void(void)> func);
    void InvokeOnRightClicked();

    // Delegate 이벤트
    Delegate::Delegate<> OnClick;
    Delegate::Delegate<> OnRightClick;
    Delegate::Delegate<> OnHoverEnter;
    Delegate::Delegate<> OnHoverExit;
    Delegate::Delegate<ButtonState> OnStateChanged;

    // 머티리얼 관리
    void SetMaterial(ButtonState state, shared_ptr<Material> material);
    void SetNormalMaterial(shared_ptr<Material> material) { SetMaterial(ButtonState::Normal, material); }
    void SetHoveredMaterial(shared_ptr<Material> material) { SetMaterial(ButtonState::Hovered, material); }
    void SetPressedMaterial(shared_ptr<Material> material) { SetMaterial(ButtonState::Pressed, material); }
    void SetDisabledMaterial(shared_ptr<Material> material) { SetMaterial(ButtonState::Disabled, material); }

    // 상태 관리
    ButtonState GetCurrentState() const { return m_currentState; }
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return m_isEnabled; }
    void SetVisible(bool visible);

    // 위치 관리
    void UpdatePosition(const Vec2& parentWorldPos);
    void SetLocalPosition(const Vec2& localPos) { m_localPosition = localPos; }
    const Vec2& GetLocalPosition() const { return m_localPosition; }
    void SetZPos(float zPos) { m_zPos = zPos; }
    RECT GetRect() const { return m_rect; }

private:
    // 핵심 로직
    void UpdateState();
    void ChangeState(ButtonState newState);
    void ApplyCurrentMaterial();
    void UpdatePickingRect(const Vec2& screenPos);

    // 헬퍼 함수들
    Vec2 CalculateWorldPosition(const Vec2& parentWorldPos) const;
    Vec2 ScreenToWorldCoords(const Vec2& screenPos) const;
    MouseInputState GetMouseInputState() const;
    ButtonState ProcessStateTransition(const MouseInputState& input) const;
    void ProcessMouseEvents(const MouseInputState& input);
    void UpdateInternalState(const MouseInputState& input);
    void SetupTransform(shared_ptr<GameObject> go, const Vec2& localPos, const Vec2& size);
    void SetupRendering(shared_ptr<GameObject> go, shared_ptr<Material> material, uint32 pass);
    shared_ptr<Material> FindMaterialForState(ButtonState state) const;

private:
    // 이벤트 콜백
    std::function<void(void)> m_onClicked;
    std::function<void(void)> m_onRightClicked;

    // 기본 속성
    Vec2 m_localPosition;
    Vec2 m_size;
    Vec2 m_materialSize;
    RECT m_rect{};
    uint32 m_pass = 0;
    float m_zPos = 0.6f;

    // 상태 관리
    ButtonState m_currentState = ButtonState::Normal;
    ButtonState m_previousState = ButtonState::Normal;
    bool m_isEnabled = true;
    bool m_visible = true;

    // 입력 상태
    bool m_isMouseInside = false;
    bool m_wasMousePressed = false;
    bool m_wasRightMousePressed = false;
    bool m_clickStartedInside = false;
    bool m_rightClickStartedInside = false;

    // 머티리얼 관리
    std::map<ButtonState, shared_ptr<Material>> m_stateMaterials;
    shared_ptr<Material> m_defaultMaterial;
};
