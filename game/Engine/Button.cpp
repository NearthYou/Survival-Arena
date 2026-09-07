#include "pch.h"
#include "Button.h"
#include "MeshRenderer.h"
#include "Material.h"

Button::Button() : Super(ComponentType::Button) {}

Button::~Button() {}

void Button::Update()
{
    Super::Update();

    if (CURSCENE->IsDestroying()) return;

    auto go = GetGameObject();
    if (!go || !m_isEnabled) return;

    UpdateState();
}

bool Button::Picked(POINT screenPos) const
{
    return ::PtInRect(&m_rect, screenPos);
}

void Button::SetVisible(bool visible)
{
    if (m_visible == visible) return;

    m_visible = visible;
    GetGameObject()->SetActive(visible);
}

void Button::Create(Vec2 localPos, Vec2 size, shared_ptr<Material> material, uint32 pass)
{
    auto go = m_gameObject.lock();
    if (!go || !material) return;

    // 크기와 머티리얼 설정
    m_size = size * RESOLUTION_CONSTANT;
    m_defaultMaterial = material;
    m_pass = pass;

    SetMaterial(ButtonState::Normal, material);

    // Transform 설정 (좌표 변환 최적화)
    SetupTransform(go, localPos, size);

    // 렌더링 컴포넌트 설정
    SetupRendering(go, material, pass);

    // Picking 영역 설정
    UpdatePickingRect(localPos);

    // 초기 상태
    ChangeState(ButtonState::Normal);
}

void Button::AddOnClickedEvent(std::function<void(void)> func)
{
    m_onClicked = std::move(func);
}

void Button::InvokeOnClicked()
{
    OnClick();
}

void Button::UpdatePosition(const Vec2& parentWorldPos)
{
    auto go = m_gameObject.lock();
    if (!go) return;

    // 새 월드 위치 계산
    Vec2 newWorldPos = CalculateWorldPosition(parentWorldPos);

    // Transform 위치 업데이트
    auto worldTransform = ScreenToWorldCoords(newWorldPos);
    go->GetTransform()->SetPosition(Vec3(worldTransform.x, worldTransform.y, m_zPos));

    // Picking RECT 업데이트
    UpdatePickingRect(newWorldPos);
}

void Button::UpdatePickingRect(const Vec2& screenPos)
{
    Vec2 halfSize = m_size * 0.5f;
    m_rect = {
        static_cast<LONG>(screenPos.x - halfSize.x),
        static_cast<LONG>(screenPos.y - halfSize.y),
        static_cast<LONG>(screenPos.x + halfSize.x),
        static_cast<LONG>(screenPos.y + halfSize.y)
    };
}

void Button::SetMaterial(ButtonState state, shared_ptr<Material> material)
{
    if (!material) return;

    m_stateMaterials[state] = material;

    if (m_currentState == state) {
        ApplyCurrentMaterial();
    }
}

void Button::SetEnabled(bool enabled)
{
    if (m_isEnabled == enabled) return;

    m_isEnabled = enabled;
    ChangeState(enabled ? ButtonState::Normal : ButtonState::Disabled);
}

void Button::UpdateState()
{
    // 입력 상태 수집 (구조체로 최적화)
    MouseInputState input = GetMouseInputState();

    // 상태 전환 로직 처리
    ButtonState newState = ProcessStateTransition(input);

    // 이벤트 처리
    ProcessMouseEvents(input);

    // 상태 업데이트
    UpdateInternalState(input);

    // 상태 변경 적용
    if (newState != m_currentState) {
        ChangeState(newState);
    }
}

void Button::ChangeState(ButtonState newState)
{
    if (m_currentState == newState) return;

    m_previousState = m_currentState;
    m_currentState = newState;

    ApplyCurrentMaterial();
    OnStateChanged(newState);
}

void Button::AddOnRightClickedEvent(std::function<void(void)> func)
{
    m_onRightClicked = std::move(func);
}

void Button::InvokeOnRightClicked()
{
    OnRightClick();
}

void Button::ApplyCurrentMaterial()
{
    if (CURSCENE->IsDestroying()) return;

    auto go = GetGameObject();
    if (!go) return;

    auto meshRenderer = go->GetMeshRenderer();
    if (!meshRenderer) return;

    // 현재 상태에 맞는 머티리얼 찾기
    shared_ptr<Material> materialToApply = FindMaterialForState(m_currentState);

    if (materialToApply) {
        meshRenderer->SetMaterial(materialToApply);
    }
}

// === Private Helper Methods ===

Vec2 Button::CalculateWorldPosition(const Vec2& parentWorldPos) const
{
    return Vec2(
        parentWorldPos.x + m_localPosition.x,
        parentWorldPos.y + m_localPosition.y
    );
}

Vec2 Button::ScreenToWorldCoords(const Vec2& screenPos) const
{
    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();

    return Vec2(
        screenPos.x - width * 0.5f,
        height * 0.5f - screenPos.y
    );
}

Button::MouseInputState Button::GetMouseInputState() const
{
    POINT mousePos = INPUT->GetMousePos();

    MouseInputState returnValue;

    returnValue.mouseInside = Picked(mousePos);
    returnValue.leftPressed = INPUT->GetButton(KEY_TYPE::LBUTTON);
    returnValue.leftDown = INPUT->GetButtonDown(KEY_TYPE::LBUTTON);
    returnValue.leftUp = INPUT->GetButtonUp(KEY_TYPE::LBUTTON);
    returnValue.rightPressed = INPUT->GetButton(KEY_TYPE::RBUTTON);
    returnValue.rightDown = INPUT->GetButtonDown(KEY_TYPE::RBUTTON);
    returnValue.rightUp = INPUT->GetButtonUp(KEY_TYPE::RBUTTON);

    return returnValue;
}

ButtonState Button::ProcessStateTransition(const MouseInputState& input) const
{
    if (input.mouseInside) {
        if (input.leftPressed || input.rightPressed) {
            return ButtonState::Pressed;
        }
        return ButtonState::Hovered;
    }
    return ButtonState::Normal;
}

void Button::ProcessMouseEvents(const MouseInputState& input)
{
    // Hover 이벤트
    if (input.mouseInside && !m_isMouseInside) {
        OnHoverEnter();
    }
    else if (!input.mouseInside && m_isMouseInside) {
        OnHoverExit();
    }

    if (input.mouseInside) {
        // 클릭 시작 플래그 설정
        if (input.leftDown) m_clickStartedInside = true;
        if (input.rightDown) m_rightClickStartedInside = true;

        // 클릭 완료 이벤트
        if (input.leftUp && m_clickStartedInside) {
            OnClick();
            m_clickStartedInside = false;
        }
        if (input.rightUp && m_rightClickStartedInside) {
            OnRightClick();
            m_rightClickStartedInside = false;
        }
    }
    else {
        // 버튼 밖에서 마우스 업 시 플래그 리셋
        if (input.leftUp) m_clickStartedInside = false;
        if (input.rightUp) m_rightClickStartedInside = false;
    }
}

void Button::UpdateInternalState(const MouseInputState& input)
{
    m_isMouseInside = input.mouseInside;
    m_wasMousePressed = input.leftPressed;
    m_wasRightMousePressed = input.rightPressed;
}

void Button::SetupTransform(shared_ptr<GameObject> go, const Vec2& localPos, const Vec2& size)
{
    auto worldCoords = ScreenToWorldCoords(localPos);
    Vec3 position(worldCoords.x, worldCoords.y, m_zPos);
    Vec3 scale(size.x * RESOLUTION_CONSTANT, size.y * RESOLUTION_CONSTANT, 1.0f);

    go->GetTransform()->SetPosition(position);
    go->GetTransform()->SetScale(scale);
    go->SetLayerIndex(LAYER_UI);
}

void Button::SetupRendering(shared_ptr<GameObject> go, shared_ptr<Material> material, uint32 pass)
{
    if (!go->GetMeshRenderer()) {
        go->AddComponent(make_shared<MeshRenderer>());
    }

    auto meshRenderer = go->GetMeshRenderer();
    meshRenderer->SetMaterial(material);
    meshRenderer->SetMesh(RESOURCES->Get<Mesh>(L"Quad"));
    meshRenderer->SetPass(pass);

    // 머티리얼 크기 캐싱
    if (auto diffuseMap = material->GetDiffuseMap()) {
        m_materialSize = diffuseMap->GetSize();
    }
}

shared_ptr<Material> Button::FindMaterialForState(ButtonState state) const
{
    auto it = m_stateMaterials.find(state);
    return (it != m_stateMaterials.end() && it->second) ?
        it->second : m_defaultMaterial;
}
