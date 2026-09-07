#pragma once
#include "Component.h"
#include "Delegate.h"

enum class SliderDirection {
    HORIZONTAL,
    VERTICAL
};
class SliderUI :
    public Component
{
    using Super = Component;

public:
    SliderUI();
    virtual ~SliderUI();

    virtual void Init() override;
    virtual void Update() override;

public:

    // 슬라이더 생성
    void Create(Vec2 localPos, Vec2 size, shared_ptr<Material> trackMaterial, shared_ptr<Material> fillMaterial, 
        shared_ptr<Material> handleMaterial, float minValue = 0.0f, float maxValue = 1.0f);


    void SetVisible(bool visible);

    // 값 설정/가져오기
    void SetValue(float value);
    float GetValue() const { return m_currentValue; }
    void SetRange(float minValue, float maxValue);

    // 방향 설정
    void SetDirection(SliderDirection direction) { m_direction = direction; }

    // 이벤트 (Delegate 패턴 사용)
    Delegate::Delegate<float> OnValueChanged;  // 값이 변경될 때
    Delegate::Delegate<float> OnDragStart;     // 드래그 시작
    Delegate::Delegate<float> OnDragEnd;       // 드래그 종료

    // UI Panel용 함수들
    void UpdatePosition(const Vec2& parentWorldPos);
    void SetLocalPosition(const Vec2& localPos) { m_localPosition = localPos; }
    const Vec2& GetLocalPosition() const { return m_localPosition; }

private:
    void HandleInput();
    void UpdateHandlePosition();
    void UpdateFillPosition();
    float ScreenToValue(float screenPos);
    float ValueToScreen(float value);
    bool IsPointInTrack(POINT point);
    bool IsPointInHandle(POINT point);

private:
    // 슬라이더 설정
    float m_minValue = 0.0f;
    float m_maxValue = 1.0f;
    float m_currentValue = 1.0f;
    SliderDirection m_direction = SliderDirection::HORIZONTAL;

    // UI 요소들
    Vec2 m_localPosition = Vec2::Zero;
    Vec2 m_worldPosition = Vec2::Zero;
    Vec2 m_trackSize = Vec2::Zero;
    Vec2 m_handleSize = Vec2(20.0f, 20.0f);

    // 상태
    bool m_isDragging = false;
    bool m_isEnabled = true;
    bool m_isDestroying = false;  // 소멸 중인지 확인하는 플래그
    bool m_visible = true;
    Vec2 m_dragOffset = Vec2::Zero;

    // 렌더링 요소들
    shared_ptr<GameObject> m_trackObject = nullptr;
    shared_ptr<GameObject> m_handleObject = nullptr;
    shared_ptr<GameObject> m_fillObject = nullptr;

    // 피킹용 RECT
    RECT m_trackRect;
    RECT m_handleRect;

    const float m_zTrackPos = 0.7f;
    const float m_zHandlePos = 0.6f;
};

