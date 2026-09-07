#pragma once
#include "Component.h"
#include <vector>

class Material;
class Mesh;
class Texture;
class UIPanel;

enum class ScrollDirection {
    Vertical,
    Horizontal,
    Both
};

class ScrollView : public Component
{
    using Super = Component;

public:
    ScrollView();
    virtual ~ScrollView();

    virtual void Init() override;
    virtual void Update() override;
    virtual void OnDestroy() override;

    // ScrollView 생성
    void Create(Vec2 screenPos, Vec2 viewSize, shared_ptr<Material> backgroundMaterial = nullptr);

    // 스크롤 설정
    void SetScrollDirection(ScrollDirection direction) { m_scrollDirection = direction; }
    void SetContentSize(Vec2 contentSize);
    void SetScrollSpeed(float speed) { m_scrollSpeed = speed; }

    // 스크롤 제어
    void ScrollTo(Vec2 position);
    void ScrollBy(Vec2 delta);
    void ScrollToTop();
    void ScrollToBottom();
    void ScrollToLeft();
    void ScrollToRight();

    // UI 요소 추가 (컨텐츠 영역에)
    shared_ptr<UIPanel> AddPanel(Vec2 localPos, Vec2 size, shared_ptr<Material> material = nullptr, const wstring& name = L"Panel");
    void AddUIElement(shared_ptr<GameObject> uiElement, Vec2 localPos);
    void RemoveUIElement(shared_ptr<GameObject> uiElement);
    void RemoveAllElement();
   
    // Getter
    Vec2 GetScrollPosition() const { return m_scrollPosition; }
    Vec2 GetViewSize() const { return m_viewSize; }
    Vec2 GetContentSize() const { return m_contentSize; }
    vector<weak_ptr<GameObject>>& GetElements() { return m_contentElements; }
    Vec4 GetClippingRect() { return m_currentClippingRect; }

    bool IsScrolling() const { return m_isScrolling; }

    // 마우스 입력 처리 (InputManager 사용)
    void HandleInput();
    bool HandleMouseWheel(int wheelDelta);

private:
    void CreateScrollView();
    void UpdateScrollPosition();
    void UpdateContentPosition();
    void ClampScrollPosition();
    bool IsPointInViewport(POINT point);
    Vec2 ScreenToContentPosition(Vec2 screenPos);

public:
    Vec2 ContentToScreenPosition(Vec2 contentPos);

    // 원래 위치 저장을 위한 헬퍼 함수
    template<typename T>
    T Clamp(T value, T min, T max) {
        return (value < min) ? min : ((value > max) ? max : value);
    }

private:
    // 기본 설정
    Vec2 m_position = Vec2::Zero;
    Vec2 m_viewSize = Vec2(400.0f, 300.0f);
    Vec2 m_contentSize = Vec2(400.0f, 600.0f);
    ScrollDirection m_scrollDirection = ScrollDirection::Vertical;
    float m_scrollSpeed = 50.0f;

    // 스크롤 상태
    Vec2 m_scrollPosition = Vec2::Zero;
    Vec2 m_maxScrollPosition = Vec2::Zero;
    bool m_isScrolling = false;
    bool m_isDragging = false;
    Vec2 m_lastMousePosition = Vec2::Zero;

    // 배경 및 컨텐츠
    shared_ptr<Material> m_backgroundMaterial = nullptr;
    shared_ptr<Mesh> m_backgroundMesh = nullptr;

    // 컨텐츠 컨테이너
    shared_ptr<GameObject> m_contentContainer = nullptr;
    vector<weak_ptr<GameObject>> m_contentElements;
    map<wstring, weak_ptr<GameObject>> m_namedElements;

    // 원래 위치 저장
    map<shared_ptr<GameObject>, Vec2> m_originalPositions;

    // 뷰포트 영역 (마스킹용)
    RECT m_viewportRect;

    // 소멸 관리
    bool m_isDestroying = false;

    const float m_zPanelPos = 0.8f;
    const float m_zScrollViewPos = 0.7f;

public:
    // 뷰포트 컬링 활성화/비활성화
    void SetViewportCulling(bool enable) { m_enableViewportCulling = enable; }
    bool IsViewportCullingEnabled() const { return m_enableViewportCulling; }

private:
    // 뷰포트 컬링 관련 함수들
    bool IsUIElementVisible(const Vec2& elementPos, const Vec2& elementSize) const;
    void UpdateUIElementVisibility();
    void UpdateChildElementsVisibility(shared_ptr<UIPanel> panel, const Vec2& panelScreenPos, const Vec2& panelSize);
    void SetChildElementsVisibility(shared_ptr<UIPanel> panel, bool visible);
    void SetGameObjectVisibility(shared_ptr<GameObject> obj, bool visible);
    void SetImageLayerVisibility(shared_ptr<GameObject> obj, bool visible);

    // 뷰포트 컬링 설정
    bool m_enableViewportCulling = true;

    
private:
    // 클리핑 관련
    bool m_enablePixelClipping = true;
   
public:
    void PushScrollViewClippingData(const Vec4& clippingRect, bool enableClipping);

public:
    void SetPixelClipping(bool enable) { m_enablePixelClipping = enable; }
    bool IsPixelClippingEnabled() const { return m_enablePixelClipping; }


    void UpdateClippingShaderData();
private:
    void SetupClippingForElement(shared_ptr<GameObject> element);
   
 private:
     // 클리핑 관련 - 각 ScrollView별 독립적 관리
     Vec4 m_currentClippingRect = Vec4::Zero;
  
     // 이 ScrollView에 속한 요소들을 추적하기 위한 식별자
     int m_scrollViewID = 0;
     static int s_nextScrollViewID;
};
