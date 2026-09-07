#include "pch.h"
#include "ScrollView.h"
#include "Transform.h"
#include "Material.h"
#include "Mesh.h"
#include "Texture.h"
#include "MeshRenderer.h"
#include "Shader.h"
#include "GameObject.h"
#include "UIPanel.h"
#include "Scene.h"
#include "SceneObjectManager.h"
#include "Graphics.h"
#include "ImageUI.h"


ScrollView::ScrollView() : Super(ComponentType::ScrollView)
{
   
}

ScrollView::~ScrollView()
{
    m_isDestroying = true;
}

void ScrollView::Init()
{
    auto go = GetGameObject();

    // MeshRenderer 추가 (배경용)
    if (go->GetMeshRenderer() == nullptr) {
        go->AddComponent(make_shared<MeshRenderer>());
    }

    // 배경 메시 생성
    m_backgroundMesh = RESOURCES->Get<Mesh>(L"Quad");
    go->GetMeshRenderer()->SetMesh(m_backgroundMesh);
    go->SetLayerIndex(LAYER_UI);

    CreateScrollView();
}

void ScrollView::Update()
{
    Super::Update();

    if (m_isDestroying) return;

    // InputManager를 사용한 입력 처리
    HandleInput();

    // 스크롤 위치 업데이트
    UpdateScrollPosition();
    UpdateContentPosition();

    // 클리핑 셰이더 데이터 업데이트
    if (m_enablePixelClipping)
    {
        UpdateClippingShaderData();
    }

    // 뷰포트 기반 UI 요소 가시성 업데이트 추가
    if (m_enableViewportCulling) {
        UpdateUIElementVisibility();
    }

    // 만료된 컨텐츠 요소 정리
    for (auto it = m_contentElements.begin(); it != m_contentElements.end();) {
        if (it->expired()) {
            it = m_contentElements.erase(it);
        }
        else {
            ++it;
        }
    }
}

void ScrollView::Create(Vec2 screenPos, Vec2 viewSize, shared_ptr<Material> backgroundMaterial)
{
    Init();

    m_position = screenPos;
    m_viewSize = viewSize;

    auto go = GetGameObject();

    // 위치 설정
    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();

    float x = screenPos.x - width / 2.0f;
    float y = height / 2.0f - screenPos.y;

    go->GetTransform()->SetPosition(Vec3(x, y, m_zScrollViewPos));
    go->GetTransform()->SetScale(Vec3(viewSize.x, viewSize.y, 1.0f));

    // 뷰포트 영역 설정
    m_viewportRect.left = static_cast<LONG>(screenPos.x - viewSize.x / 2.0f);
    m_viewportRect.right = static_cast<LONG>(screenPos.x + viewSize.x / 2.0f);
    m_viewportRect.top = static_cast<LONG>(screenPos.y - viewSize.y / 2.0f);
    m_viewportRect.bottom = static_cast<LONG>(screenPos.y + viewSize.y / 2.0f);

    // 배경 머티리얼 설정
    if (backgroundMaterial) {
        m_backgroundMaterial = backgroundMaterial;

        go->GetMeshRenderer()->SetMaterial(m_backgroundMaterial);
        go->GetMeshRenderer()->SetPass(1);
    }
    else {
        // 기본 배경 머티리얼 생성
        m_backgroundMaterial = make_shared<Material>();
        auto shader = make_shared<Shader>(L"ImageShader.fx");
        m_backgroundMaterial->SetShader(shader);
        m_backgroundMaterial->SetRenderQueue(RenderQueue::Transparent);
        m_backgroundMaterial->SetTransparent(true);
        m_backgroundMaterial->SetRenderingMode(RenderingMode::Forward);

        MaterialDesc& desc = m_backgroundMaterial->GetMaterialDesc();
        desc.ambient = Vec4(0.f, 0.f, 0.f, 0.7f);
        desc.diffuse = Vec4(0.f, 0.f, 0.f, 0.7f);
        desc.specular = Vec4(0.f);
        desc.emissive = Vec4(0.f);

        go->GetMeshRenderer()->SetMaterial(m_backgroundMaterial);
        go->GetMeshRenderer()->SetPass(2);
    }
}

void ScrollView::SetContentSize(Vec2 contentSize)
{
    m_contentSize = contentSize;

    // 최대 스크롤 위치 계산
    m_maxScrollPosition.x = max(0.0f, m_contentSize.x - m_viewSize.x);
    m_maxScrollPosition.y = max(0.0f, m_contentSize.y - m_viewSize.y);

    ClampScrollPosition();
}

void ScrollView::ScrollTo(Vec2 position)
{
    m_scrollPosition = position;
    ClampScrollPosition();
}

void ScrollView::ScrollBy(Vec2 delta)
{
    m_scrollPosition += delta;
    ClampScrollPosition();
}

void ScrollView::ScrollToTop()
{
    m_scrollPosition.y = 0.0f;
}

void ScrollView::ScrollToBottom()
{
    m_scrollPosition.y = m_maxScrollPosition.y;
}

void ScrollView::ScrollToLeft()
{
    m_scrollPosition.x = 0.0f;
}

void ScrollView::ScrollToRight()
{
    m_scrollPosition.x = m_maxScrollPosition.x;
}

shared_ptr<UIPanel> ScrollView::AddPanel(Vec2 localPos, Vec2 size, shared_ptr<Material> material, const wstring& name)
{
    if (m_isDestroying) return nullptr;

    // 패널 GameObject 생성
    auto panelObj = make_shared<GameObject>();
    panelObj->GetTransform()->SetPosition(Vec3(localPos.x, localPos.y, m_zPanelPos));
    // 패널에 ScrollView ID 태그 추가 (옵션)
    panelObj->SetName(name);
    // UIPanel 컴포넌트 추가
    auto panelComponent = make_shared<UIPanel>();
    panelObj->AddComponent(panelComponent);

    // 원래 위치 저장
    m_originalPositions[panelObj] = localPos;

    // 컨텐츠 좌표를 화면 좌표로 변환
    Vec2 screenPos = ContentToScreenPosition(localPos);
    panelComponent->Create(screenPos, size, Vec4(0.f), material);

    panelObj->SetLayerIndex(LAYER_UI);

    // 컨텐츠 요소로 등록
    m_contentElements.push_back(panelObj);
    m_namedElements[name] = panelObj;

    // UI 객체로 씬에 추가
    CURSCENE->AddUIObject(panelObj, false);
    CURSCENE->RegisterUIChild(panelObj);

    return panelComponent;
}

void ScrollView::AddUIElement(shared_ptr<GameObject> uiElement, Vec2 localPos)
{
    if (m_isDestroying || !uiElement) return;

    // 원래 위치 저장 (중요!)
    m_originalPositions[uiElement] = localPos;

    // 컨텐츠 좌표를 화면 좌표로 변환하여 위치 설정
    Vec2 screenPos = ContentToScreenPosition(localPos);

    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();

    float x = screenPos.x - width / 2.0f;
    float y = height / 2.0f - screenPos.y;

    uiElement->GetTransform()->SetPosition(Vec3(x, y, -0.1f));
    uiElement->SetLayerIndex(LAYER_UI);

    // 컨텐츠 요소로 등록
    m_contentElements.push_back(uiElement);

    // UI 객체로 씬에 추가
    CURSCENE->AddUIObject(uiElement, false);
    CURSCENE->RegisterUIChild(uiElement);
}

void ScrollView::RemoveUIElement(shared_ptr<GameObject> uiElement)
{
    if (m_isDestroying || !uiElement) return;

    // 컨텐츠 요소에서 제거
    auto it = std::find_if(m_contentElements.begin(), m_contentElements.end(),
        [&uiElement](const weak_ptr<GameObject>& weakPtr) {
            return !weakPtr.owner_before(uiElement) && !uiElement.owner_before(weakPtr);
        });

    if (it != m_contentElements.end()) {
        m_contentElements.erase(it);
    }

    // 지연 삭제 사용
    if (CURSCENE && !CURSCENE->IsDestroying()) {
        CURSCENE->GetObjectManager()->MarkUIObjectForDestroy(uiElement);
    }
}

void ScrollView::RemoveAllElement()
{
    if (m_isDestroying) return;

    // 모든 컨텐츠 요소들을 지연 삭제로 처리
    for (auto it = m_contentElements.begin(); it != m_contentElements.end(); ++it)
    {
        if (auto element = it->lock())
        {
            // UIPanel인 경우 자식 요소들까지 계층적으로 삭제
            if (auto uiPanel = element->GetUIPanel())
            {
                // UIPanel의 계층적 삭제 사용
                if (CURSCENE && !CURSCENE->IsDestroying()) {
                    CURSCENE->GetObjectManager()->MarkUIObjectForDestroyWithChildren(element);
                }
            }
            else
            {
                // 일반 UI 요소 삭제
                if (CURSCENE && !CURSCENE->IsDestroying()) {
                    CURSCENE->GetObjectManager()->MarkUIObjectForDestroy(element);
                }
            }
        }
    }

    // 컨테이너들 정리
    m_contentElements.clear();
    m_namedElements.clear();
    m_originalPositions.clear();

    // 스크롤 위치 초기화
    m_scrollPosition = Vec2::Zero;

#if _DEBUG
    std::wcout << L"ScrollView: 모든 UI 요소가 삭제되었습니다." << std::endl;
#endif
}


void ScrollView::HandleInput()
{
    // InputManager를 사용한 마우스 입력 처리
    POINT mousePos = INPUT->GetMousePos();
    bool isPressed = INPUT->GetButton(KEY_TYPE::LBUTTON);
    bool isDown = INPUT->GetButtonDown(KEY_TYPE::LBUTTON);
    bool isUp = INPUT->GetButtonUp(KEY_TYPE::LBUTTON);

    // 마우스 휠 처리
    if (INPUT->HasWheelInput()) {
        if (IsPointInViewport(mousePos)) {
            int wheelDelta = INPUT->GetMouseWheelDelta();
            bool handled = HandleMouseWheel(wheelDelta);
        }

        // 휠 이벤트 소비
        INPUT->ResetWheelDelta();
    }

    // 기존 드래그 처리
    if (IsPointInViewport(mousePos)) {
        if (isDown && !m_isDragging) {
            m_isDragging = true;
            m_lastMousePosition = Vec2(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
        }
    }

    if (m_isDragging) {
        if (isUp) {
            m_isDragging = false;
        }
        else if (isPressed) {
            Vec2 currentMousePos = Vec2(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
            Vec2 delta = currentMousePos - m_lastMousePosition;

            Vec2 scrollDelta = Vec2::Zero;
            if (m_scrollDirection == ScrollDirection::Vertical || m_scrollDirection == ScrollDirection::Both) {
                scrollDelta.y = -delta.y;
            }
            if (m_scrollDirection == ScrollDirection::Horizontal || m_scrollDirection == ScrollDirection::Both) {
                scrollDelta.x = -delta.x;
            }

            if (scrollDelta.x != 0.0f || scrollDelta.y != 0.0f) {
                ScrollBy(scrollDelta);
            }
            m_lastMousePosition = currentMousePos;
        }
    }
}

void ScrollView::CreateScrollView()
{
    // 컨텐츠 컨테이너는 실제로는 가상적인 개념
    // 실제 UI 요소들의 위치를 스크롤 위치에 따라 조정
}

void ScrollView::UpdateScrollPosition()
{
    ClampScrollPosition();
}

void ScrollView::UpdateContentPosition()
{
    if (m_isDestroying) return;

    for (auto it = m_contentElements.begin(); it != m_contentElements.end();) {
        if (auto element = it->lock()) {
            // 원래 위치에서 스크롤 오프셋 적용
            auto posIt = m_originalPositions.find(element);
            if (posIt != m_originalPositions.end()) {
                Vec2 originalContentPos = posIt->second;
                Vec2 screenPos = ContentToScreenPosition(originalContentPos);

                //cout << "아마 panel : " << screenPos.x << " , " << screenPos.y << endl;

                float height = GRAPHICS->GetViewport().GetHeight();
                float width = GRAPHICS->GetViewport().GetWidth();

                float x = screenPos.x - width / 2.0f;
                float y = height / 2.0f - screenPos.y;

                element->GetTransform()->SetPosition(Vec3(x, y, element->GetTransform()->GetPosition().z));
            }
            ++it;
        }
        else {
            it = m_contentElements.erase(it);
        }
    }
}

void ScrollView::ClampScrollPosition()
{
    m_scrollPosition.x = Clamp(m_scrollPosition.x, 0.0f, m_maxScrollPosition.x);
    m_scrollPosition.y = Clamp(m_scrollPosition.y, 0.0f, m_maxScrollPosition.y);
}

bool ScrollView::IsPointInViewport(POINT point)
{
    return PtInRect(&m_viewportRect, point);
}

Vec2 ScrollView::ScreenToContentPosition(Vec2 screenPos)
{
    // 화면 좌표를 컨텐츠 로컬 좌표로 변환
    Vec2 viewportLocalPos = screenPos - m_position;
    Vec2 contentPos = viewportLocalPos + m_scrollPosition;
    return contentPos;
}

Vec2 ScrollView::ContentToScreenPosition(Vec2 contentPos)
{
    // 컨텐츠 로컬 좌표를 화면 좌표로 변환
    Vec2 viewportLocalPos = contentPos - m_scrollPosition;
    Vec2 screenPos = viewportLocalPos + m_position;
    return screenPos;
}

void ScrollView::OnDestroy()
{
    m_isDestroying = true;

    // 컨텐츠 요소들 정리
    for (auto& weakElement : m_contentElements) {
        if (auto element = weakElement.lock()) {
            if (CURSCENE && !CURSCENE->IsDestroying()) {
                CURSCENE->GetObjectManager()->MarkUIObjectForDestroy(element);
            }
        }
    }

    // 컨테이너 정리
    m_contentElements.clear();
    m_namedElements.clear();

    Super::OnDestroy();
}

bool ScrollView::HandleMouseWheel(int wheelDelta)
{
    POINT mousePos = INPUT->GetMousePos();
    if (!IsPointInViewport(mousePos)) return false;

    float scrollAmount = (wheelDelta > 0) ? -m_scrollSpeed : m_scrollSpeed;
    Vec2 scrollDelta = Vec2::Zero;

    if (m_scrollDirection == ScrollDirection::Vertical || m_scrollDirection == ScrollDirection::Both) {
        scrollDelta.y = scrollAmount;
    }
    else
    {
        scrollDelta.x = scrollAmount;
    }

    ScrollBy(scrollDelta);
    return true;
}


// 뷰포트 영역 내에 UI 요소가 있는지 체크하는 함수
bool ScrollView::IsUIElementVisible(const Vec2& elementPos, const Vec2& elementSize) const
{
    // 뷰포트 영역 계산 (스크롤 고려)
    Vec2 viewportMin = m_position - m_viewSize * 0.5f;
    Vec2 viewportMax = m_position + m_viewSize * 0.5f;

    // UI 요소의 영역 계산
    Vec2 elementMin = elementPos - elementSize * 0.5f;
    Vec2 elementMax = elementPos + elementSize * 0.5f;

    // AABB 충돌 체크 (겹치는 부분이 있으면 보임)
    return !(elementMax.x < viewportMin.x || elementMin.x > viewportMax.x ||
        elementMax.y < viewportMin.y || elementMin.y > viewportMax.y);
}

// UI 요소들의 가시성을 업데이트하는 함수
void ScrollView::UpdateUIElementVisibility()
{
    for (auto it = m_contentElements.begin(); it != m_contentElements.end(); ++it) {
        if (auto element = it->lock()) {
            // UIPanel인지 확인
            if (auto uiPanel = element->GetUIPanel()) {
                // UIPanel의 화면 좌표와 크기 계산
                Vec3 panelWorldPos = element->GetTransform()->GetPosition();
                Vec3 panelScale = element->GetTransform()->GetScale();

                // 월드 좌표를 화면 좌표로 변환
                float width = GRAPHICS->GetViewport().GetWidth();
                float height = GRAPHICS->GetViewport().GetHeight();

                Vec2 panelScreenPos = Vec2(
                    panelWorldPos.x + width / 2.0f,
                    height / 2.0f - panelWorldPos.y
                );

                Vec2 panelSize = Vec2(panelScale.x, panelScale.y);

                // UIPanel이 뷰포트 영역에 보이는지 체크
                bool isPanelVisible = IsUIElementVisible(panelScreenPos, panelSize);

                // UIPanel의 가시성 설정
                SetGameObjectVisibility(element, isPanelVisible);

                // UIPanel의 자식 요소들도 함께 처리
                if (isPanelVisible) {
                    // 패널이 보이면 자식들의 개별 가시성도 체크
                    UpdateChildElementsVisibility(uiPanel, panelScreenPos, panelSize);
                }
                else {
                    // 패널이 안 보이면 모든 자식들도 숨김
                    SetChildElementsVisibility(uiPanel, false);
                }
            }
        }
    }
}


void ScrollView::UpdateChildElementsVisibility(shared_ptr<UIPanel> panel, const Vec2& panelScreenPos, const Vec2& panelSize)
{
    const auto& childElements = panel->GetChildElements();

    for (const auto& weakChild : childElements)
    {
        if (auto child = weakChild.lock())
        {
            Vec3 childWorldPos = child->GetTransform()->GetPosition();
            Vec3 childScale = child->GetTransform()->GetScale();

            // 화면 좌표 변환을 일관되게 처리
            float width = GRAPHICS->GetViewport().GetWidth();
            float height = GRAPHICS->GetViewport().GetHeight();

            Vec2 childScreenPos = Vec2(
                childWorldPos.x + width / 2.0f,
                height / 2.0f - childWorldPos.y
            );

            if (child->GetImageUI())
            {
                auto layers = child->GetImageUI()->GetLayers();
                for (auto& layer : layers)
                {
                    // ImageLayer의 위치는 child의 위치를 기준으로 상대적 오프셋
                    Vec2 layerScreenPos = Vec2(
                        childScreenPos.x + layer.second.position.x,
                        childScreenPos.y + layer.second.position.y
                    );

                    Vec2 layerSize = layer.second.size;

                    // Layer별로 개별 가시성 체크
                    bool isLayerVisible = IsUIElementVisible(layerScreenPos, layerSize);
                 
                    if (layer.second.gameObject && layer.second.gameObject->GetMeshRenderer()) {
                        layer.second.gameObject->GetMeshRenderer()->SetActive(isLayerVisible);
                    }
                }
            }
            else
            {
                Vec2 childSize = Vec2(childScale.x, childScale.y);
                bool isChildVisible = IsUIElementVisible(childScreenPos, childSize);
                SetGameObjectVisibility(child, isChildVisible);
            }
        }
    }
}


// UIPanel의 모든 자식 요소들 가시성 일괄 설정
void ScrollView::SetChildElementsVisibility(shared_ptr<UIPanel> panel, bool visible)
{
    const auto& childElements = panel->GetChildElements();

    for (const auto& weakChild : childElements) 
    {
        if (auto child = weakChild.lock()) 
        {
            if (child->GetImageUI())
            {
                SetImageLayerVisibility(child, visible);
            }
            else
            {
                SetGameObjectVisibility(child, visible);
            }
        }
    }
}

// GameObject의 가시성을 설정하는 헬퍼 함수
void ScrollView::SetGameObjectVisibility(shared_ptr<GameObject> obj, bool visible)
{
    if (!obj) return;

    // MeshRenderer가 있으면 활성화/비활성화
    if (auto meshRenderer = obj->GetMeshRenderer()) {
        meshRenderer->SetActive(visible);
    }
}

// GameObject의 가시성을 설정하는 헬퍼 함수
void ScrollView::SetImageLayerVisibility(shared_ptr<GameObject> obj, bool visible)
{
    if (!obj) return;

    // MeshRenderer가 있으면 활성화/비활성화
    if (auto meshRenderer = obj->GetMeshRenderer()) {
        meshRenderer->SetActive(visible);
    }

    map<int, ImageLayer>& layers = obj->GetImageUI()->GetLayers();

    auto iter = layers.begin();
    for (; iter != layers.end(); iter++)
    {
        iter->second.gameObject->GetMeshRenderer()->SetActive(visible);
    }  
}



void ScrollView::UpdateClippingShaderData()
{
    //// 클리핑 영역을 스크린 좌표로 설정
    //Vec4 clippingRect;
    //clippingRect.x = m_position.x - m_viewSize.x / 2.0f; // left
    //clippingRect.y = m_position.y - m_viewSize.y / 2.0f; // top
    //clippingRect.z = m_position.x + m_viewSize.x / 2.0f; // right
    //clippingRect.w = m_position.y + m_viewSize.y / 2.0f; // bottom

    //// 모든 컨텐츠 요소에 클리핑 데이터 적용
    //for (auto it = m_contentElements.begin(); it != m_contentElements.end(); ++it)
    //{
    //    if (auto element = it->lock())
    //    {
    //        SetupClippingForElement(element);
    //    }
    //}

    //if (!m_enablePixelClipping) return;

    //// **오직 이 ScrollView의 요소들에만 클리핑 적용**
    //for (auto it = m_contentElements.begin(); it != m_contentElements.end(); ++it)
    //{
    //    if (auto element = it->lock())
    //    {
    //        SetupClippingForElement(element);
    //    }
    //}



    if (!m_enablePixelClipping) return;

    // 현재 클리핑 영역 계산
    m_currentClippingRect.x = m_position.x - m_viewSize.x / 2.0f;
    m_currentClippingRect.y = m_position.y - m_viewSize.y / 2.0f;
    m_currentClippingRect.z = m_position.x + m_viewSize.x / 2.0f;
    m_currentClippingRect.w = m_position.y + m_viewSize.y / 2.0f;

    // 이 ScrollView에 속한 요소들에만 클리핑 적용
    for (auto it = m_contentElements.begin(); it != m_contentElements.end(); ++it)
    {
        if (auto element = it->lock())
        {
            //ApplyScrollViewClipping(element, m_currentClippingRect);
            SetupClippingForElement(element);
        }
    }
}

void ScrollView::SetupClippingForElement(shared_ptr<GameObject> element)
{
    if (!element) return;

    // MeshRenderer가 있는 경우
    if (auto meshRenderer = element->GetMeshRenderer())
    {
        auto material = meshRenderer->GetMaterial();
        if (material && material->GetShader())
        {
            auto shader = material->GetShader();

            // 클리핑 영역 설정
            Vec4 clippingRect;
            clippingRect.x = m_position.x - m_viewSize.x / 2.0f;
            clippingRect.y = m_position.y - m_viewSize.y / 2.0f;
            clippingRect.z = m_position.x + m_viewSize.x / 2.0f;
            clippingRect.w = m_position.y + m_viewSize.y / 2.0f;

            // 개별 상수 버퍼에 클리핑 데이터 설정
            shader->PushScrollViewClippingData(clippingRect, m_enablePixelClipping);

            // 클리핑 패스 사용
            if (element->GetText())
                meshRenderer->SetPass(1);
            else
                meshRenderer->SetPass(4);
        }
    }

    // UIPanel인 경우 자식 요소들도 처리
    if (auto uiPanel = element->GetUIPanel())
    {
        const auto& childElements = uiPanel->GetChildElements();
        for (const auto& weakChild : childElements)
        {
            if (auto child = weakChild.lock())
            {
                SetupClippingForElement(child);

                // ImageUI의 레이어들도 처리
                if (auto imageUI = child->GetImageUI())
                {
                    auto& layers = imageUI->GetLayers();
                    for (auto& layer : layers)
                    {
                        if (layer.second.gameObject)
                        {
                            SetupClippingForElement(layer.second.gameObject);
                        }
                    }
                }
            }
        }
    }
}

