#include "pch.h"
#include "SliderUI.h"
#include "MeshRenderer.h"

SliderUI::SliderUI() : Super(ComponentType::SLIDER)
{
}

SliderUI::~SliderUI()
{
    m_isDestroying = true;
}

void SliderUI::Init()
{
    auto go = GetGameObject();

    if (go->GetMeshRenderer() == nullptr) {
        go->AddComponent(make_shared<MeshRenderer>());
    }

    go->SetLayerIndex(LAYER_UI);
}

void SliderUI::Update()
{
    Super::Update();

    if (m_isDestroying || !m_isEnabled || !m_visible)
        return;

    HandleInput();
}

void SliderUI::Create(Vec2 localPos, Vec2 size, shared_ptr<Material> trackMaterial, shared_ptr<Material> fillMaterial, shared_ptr<Material> handleMaterial, float minValue, float maxValue)
{
    // 이 파라미터는 UIPanel이 LocalToWorldPosition(...) 결과를 넘기고 있기 때문에
    // 여기서는 world position으로 저장한다.
    m_worldPosition = localPos;
    m_trackSize = size;
    m_minValue = minValue;
    m_maxValue = maxValue;
    m_currentValue = 1.f;

    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();

    // Track GameObject 생성
    m_trackObject = make_shared<GameObject>();
    m_trackObject->SetName(L"SliderTrack");
    m_trackObject->SetLayerIndex(LAYER_UI);
    m_trackObject->AddComponent(make_shared<MeshRenderer>());

    auto mesh = RESOURCES->Get<Mesh>(L"Quad");
    m_trackObject->GetMeshRenderer()->SetMesh(mesh);
    m_trackObject->GetMeshRenderer()->SetMaterial(trackMaterial);
    m_trackObject->GetMeshRenderer()->SetPass(0);

    float trackX = m_worldPosition.x - width / 2.0f;
    float trackY = height / 2.0f - m_worldPosition.y;
    m_trackObject->GetTransform()->SetPosition(Vec3(trackX, trackY, m_zTrackPos));
    m_trackObject->GetTransform()->SetScale(Vec3(m_trackSize.x, m_trackSize.y, 1));

    m_fillObject = make_shared<GameObject>();
    m_fillObject->SetName(L"SliderFill");
    m_fillObject->SetLayerIndex(LAYER_UI);
    m_fillObject->AddComponent(make_shared<MeshRenderer>());
    m_fillObject->GetMeshRenderer()->SetMesh(mesh);
    m_fillObject->GetMeshRenderer()->SetMaterial(fillMaterial);
    m_fillObject->GetMeshRenderer()->SetPass(0);

    // Handle GameObject 생성
    m_handleObject = make_shared<GameObject>();
    m_handleObject->SetName(L"SliderHandle");
    m_handleObject->SetLayerIndex(LAYER_UI);
    m_handleObject->AddComponent(make_shared<MeshRenderer>());

    m_handleObject->GetMeshRenderer()->SetMesh(mesh);
    m_handleObject->GetMeshRenderer()->SetMaterial(handleMaterial);
    m_handleObject->GetMeshRenderer()->SetPass(0);
    m_handleObject->GetTransform()->SetScale(Vec3(m_handleSize.x, m_handleSize.y, 1));
    m_handleObject->GetTransform()->SetPosition(Vec3(trackX, trackY, m_zHandlePos));

    // Scene에 UI 객체로 등록
    if (CURSCENE) {
        CURSCENE->AddUIObject(m_trackObject, false);
        CURSCENE->RegisterUIChild(m_trackObject);

        CURSCENE->AddUIObject(m_fillObject, false);
        CURSCENE->RegisterUIChild(m_fillObject);

        CURSCENE->AddUIObject(m_handleObject, false);
        CURSCENE->RegisterUIChild(m_handleObject);
    }

    // 피킹용 RECT 초기화 (screen-space, origin = top-left)
    m_trackRect.left = static_cast<LONG>(m_worldPosition.x - m_trackSize.x / 2);
    m_trackRect.right = static_cast<LONG>(m_worldPosition.x + m_trackSize.x / 2);
    m_trackRect.top = static_cast<LONG>(m_worldPosition.y - m_trackSize.y / 2);
    m_trackRect.bottom = static_cast<LONG>(m_worldPosition.y + m_trackSize.y / 2);

    UpdateHandlePosition();
    UpdateFillPosition();

    m_isEnabled = true;
    m_visible = true;
    m_isDragging = false;
}

void SliderUI::SetVisible(bool visible)
{
    m_visible = visible;
    GetGameObject()->SetActive(visible);

    m_trackObject->SetActive(visible);
    m_handleObject->SetActive(visible);
    m_fillObject->SetActive(visible);
}

void SliderUI::SetValue(float value)
{
    float clampedValue = std::clamp(value, m_minValue, m_maxValue);

    if (fabs(clampedValue - m_currentValue) > 0.0001f) {
        m_currentValue = clampedValue;
        UpdateHandlePosition();
        UpdateFillPosition();
        OnValueChanged(m_currentValue);
    }
}

void SliderUI::SetRange(float minValue, float maxValue)
{
    m_minValue = minValue;
    m_maxValue = maxValue;

    // 현재 값이 범위를 벗어나면 클램프
    if (m_currentValue < m_minValue || m_currentValue > m_maxValue) {
        SetValue(m_currentValue);
    }
}

void SliderUI::UpdatePosition(const Vec2& parentWorldPos)
{
    Vec2 newWorldPos;
    newWorldPos.x = parentWorldPos.x + m_localPosition.x;
    newWorldPos.y = parentWorldPos.y + m_localPosition.y;

    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();

    // 트랙 위치 업데이트 (world position 기준)
    if (m_trackObject) {
        float x = newWorldPos.x - width / 2.0f;
        float y = height / 2.0f - newWorldPos.y;
        m_trackObject->GetTransform()->SetPosition(Vec3(x, y, m_zTrackPos));
    }

    // **중요**: m_localPosition을 newWorldPos로 덮어쓰지 않는다.
    m_worldPosition = newWorldPos;

    UpdateHandlePosition();
    UpdateFillPosition();

    // 트랙 피킹 RECT 업데이트 (screen-space)
    m_trackRect.left = static_cast<LONG>(m_worldPosition.x - m_trackSize.x / 2);
    m_trackRect.right = static_cast<LONG>(m_worldPosition.x + m_trackSize.x / 2);
    m_trackRect.top = static_cast<LONG>(m_worldPosition.y - m_trackSize.y / 2);
    m_trackRect.bottom = static_cast<LONG>(m_worldPosition.y + m_trackSize.y / 2);
}

void SliderUI::HandleInput()
{
    if (!m_handleObject || !m_trackObject) return;

    POINT mousePos = INPUT->GetMousePos();
    bool leftDown = INPUT->GetButtonDown(KEY_TYPE::LBUTTON);
    bool leftPressed = INPUT->GetButton(KEY_TYPE::LBUTTON);
    bool leftUp = INPUT->GetButtonUp(KEY_TYPE::LBUTTON);

    if (!m_isDragging) {
        if (leftDown && IsPointInHandle(mousePos)) {
            m_isDragging = true;

            // 드래그 오프셋 계산
            float mouseScreenPos = (m_direction == SliderDirection::HORIZONTAL) ?
                (float)mousePos.x : (float)mousePos.y;
            float handleScreenPos = ValueToScreen(m_currentValue);

            if (m_direction == SliderDirection::HORIZONTAL) {
                m_dragOffset.x = mouseScreenPos - handleScreenPos;
                m_dragOffset.y = 0;
            }
            else {
                m_dragOffset.x = 0;
                m_dragOffset.y = mouseScreenPos - handleScreenPos;
            }

            OnDragStart(m_currentValue);
        }
        else if (leftDown && IsPointInTrack(mousePos)) {
            // 트랙 클릭 시 핸들을 해당 위치로 점프
            float pos = (m_direction == SliderDirection::HORIZONTAL) ?
                (float)mousePos.x : (float)mousePos.y;
            float newValue = ScreenToValue(pos);
            SetValue(newValue);
        }
    }
    else {
        if (leftPressed) {
            // 드래그 중
            float pos = (m_direction == SliderDirection::HORIZONTAL) ?
                (float)mousePos.x : (float)mousePos.y;
            pos -= (m_direction == SliderDirection::HORIZONTAL) ?
                m_dragOffset.x : m_dragOffset.y;

            float newValue = ScreenToValue(pos);
            SetValue(newValue);
        }

        if (leftUp) {
            // 드래그 종료
            m_isDragging = false;
            OnDragEnd(m_currentValue);
        }
    }
}

void SliderUI::UpdateHandlePosition()
{
    if (!m_handleObject) return;

    float screenPos = ValueToScreen(m_currentValue); // screen-space scalar
    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();

    float x_screen, y_screen; // screen-space (0,0 = top-left)
    if (m_direction == SliderDirection::HORIZONTAL) {
        x_screen = screenPos;
        y_screen = m_worldPosition.y;
    }
    else {
        x_screen = m_worldPosition.x;
        y_screen = screenPos;
    }

    // 엔진 Transform 좌표로 변환 (center-origin)
    float x = x_screen - width / 2.0f;
    float y = height / 2.0f - y_screen;

    m_handleObject->GetTransform()->SetPosition(Vec3(x, y, m_zHandlePos));

    // 피킹용 RECT 업데이트 (screen-space 기준)
    m_handleRect.left = static_cast<LONG>(x_screen - m_handleSize.x / 2);
    m_handleRect.right = static_cast<LONG>(x_screen + m_handleSize.x / 2);
    m_handleRect.top = static_cast<LONG>(y_screen - m_handleSize.y / 2);
    m_handleRect.bottom = static_cast<LONG>(y_screen + m_handleSize.y / 2);
}

void SliderUI::UpdateFillPosition()
{
    if (!m_fillObject) return;
    //if (m_direction == SliderDirection::HORIZONTAL)
    //{
    //    float percent = (m_currentValue - m_minValue) / (m_maxValue - m_minValue);
    //    m_fillObject->GetTransform()->SetScale(Vec3(m_trackSize.x * percent, m_trackSize.y, 1));
    //}
    //else
    //{
    //    float percent = (m_currentValue - m_minValue) / (m_maxValue - m_minValue);
    //    m_fillObject->GetTransform()->SetScale(Vec3(m_trackSize.x, m_trackSize.y * percent, 1));
    //}

    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();
    float percent = (m_currentValue - m_minValue) / (m_maxValue - m_minValue);

    if (m_direction == SliderDirection::HORIZONTAL)
    {
        // 왼쪽에서 시작하는 방식
        float fillWidth = m_trackSize.x * percent;

        // 크기 설정
        m_fillObject->GetTransform()->SetScale(Vec3(fillWidth, m_trackSize.y, 1));

        float trackLeftX = m_worldPosition.x - m_trackSize.x / 2.0f;
        float fillCenterX = trackLeftX + fillWidth / 2.0f;

        float fillX = fillCenterX - width / 2.0f; 
        float fillY = height / 2.0f - m_worldPosition.y;

        m_fillObject->GetTransform()->SetPosition(Vec3(fillX, fillY, m_zTrackPos - 0.01f));
    }
    else // VERTICAL
    {
        //  아래쪽에서 시작하는 방식
        float fillHeight = m_trackSize.y * percent;

        // 크기 설정
        m_fillObject->GetTransform()->SetScale(Vec3(m_trackSize.x, fillHeight, 1));

        float trackBottomY = m_worldPosition.y + m_trackSize.y / 2.0f;
        float fillCenterY = trackBottomY - fillHeight / 2.0f;

        float fillX = m_worldPosition.x - width / 2.0f;
        float fillY = height / 2.0f - fillCenterY;

        m_fillObject->GetTransform()->SetPosition(Vec3(fillX, fillY, m_zTrackPos - 0.01f));
    }
}


float SliderUI::ScreenToValue(float screenPos)
{
    float trackStart, trackEnd;

    if (m_direction == SliderDirection::HORIZONTAL) {
        trackStart = m_worldPosition.x - m_trackSize.x / 2.0f;
        trackEnd = m_worldPosition.x + m_trackSize.x / 2.0f;
    }
    else {
        trackStart = m_worldPosition.y - m_trackSize.y / 2.0f;
        trackEnd = m_worldPosition.y + m_trackSize.y / 2.0f;
    }

    // 클램프
    screenPos = std::clamp(screenPos, trackStart, trackEnd);

    float t = (screenPos - trackStart) / (trackEnd - trackStart);

    // 세로 슬라이더의 경우 Y축 반전
    if (m_direction == SliderDirection::VERTICAL) {
        t = 1.0f - t;
    }

    return m_minValue + t * (m_maxValue - m_minValue);
}

float SliderUI::ValueToScreen(float value)
{
    float trackStart, trackEnd;

    if (m_direction == SliderDirection::HORIZONTAL) {
        trackStart = m_worldPosition.x - m_trackSize.x / 2.0f;
        trackEnd = m_worldPosition.x + m_trackSize.x / 2.0f;
    }
    else {
        trackStart = m_worldPosition.y - m_trackSize.y / 2.0f;
        trackEnd = m_worldPosition.y + m_trackSize.y / 2.0f;
    }

    float t = (value - m_minValue) / (m_maxValue - m_minValue);

    // 세로 슬라이더의 경우 Y축 반전
    if (m_direction == SliderDirection::VERTICAL) {
        t = 1.0f - t;
    }

    return trackStart + t * (trackEnd - trackStart);
}

bool SliderUI::IsPointInTrack(POINT point)
{
    return ::PtInRect(&m_trackRect, point);
}

bool SliderUI::IsPointInHandle(POINT point)
{
    return ::PtInRect(&m_handleRect, point);
}
