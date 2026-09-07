#include "pch.h"
#include "UIPanel.h"
#include "Transform.h"
#include "Material.h"
#include "Mesh.h"
#include "Texture.h"
#include "MeshRenderer.h"
#include "Shader.h"
#include "Button.h"
#include "Text.h"
#include "GameObject.h"
#include "ImageUI.h"
#include "SliderUI.h"
#include "SceneManager.h"
#include "Scene.h"
#include "SceneObjectManager.h"

const float Z_UIPANEL = 0.8f;         
const float Z_BUTTON = 0.6f;                  
const float Z_IMAGEUI_BASE = 0.4f;    
const float Z_TEXT = 0.2f; 

UIPanel::UIPanel() : Super(ComponentType::UIPanel)
{
}

UIPanel::~UIPanel()
{
    m_isDestroying = true;
    ClearChildReferences();
}

void UIPanel::Init()
{
    auto go = GetGameObject();

    // MeshRenderer 추가 (배경용)
    if (go->GetMeshRenderer() == nullptr) {
        go->AddComponent(make_shared<MeshRenderer>());
    }

    // 배경 메시 생성
    CreatePanelBackground();

    // UI 레이어 설정
    go->SetLayerIndex(LAYER_UI);
}

void UIPanel::Update()
{
    Super::Update();

    // Transform 위치와 m_position 동기화
    auto go = GetGameObject();
    if (go) {
        Vec3 currentWorldPos = go->GetTransform()->GetPosition();

        // 월드 좌표를 화면 좌표로 변환
        float height = GRAPHICS->GetViewport().GetHeight();
        float width = GRAPHICS->GetViewport().GetWidth();

        float screenX = currentWorldPos.x + width / 2.0f;
        float screenY = height / 2.0f - currentWorldPos.y;

        Vec2 newScreenPos = Vec2(screenX, screenY);

        // 위치가 변경되었다면 업데이트
        if (abs(newScreenPos.x - m_position.x) > 0.1f || abs(newScreenPos.y - m_position.y) > 0.1f) {
            m_position = newScreenPos;
            UpdateChildPositions();
        }
    }

    // 드래그 처리 추가
    HandleDragInput();

    m_rect.left = static_cast<LONG>(m_position.x - m_size.x * 0.5f);
    m_rect.top = static_cast<LONG>(m_position.y - m_size.y * 0.5f);
    m_rect.right = static_cast<LONG>(m_position.x + m_size.x * 0.5f);
    m_rect.bottom = static_cast<LONG>(m_position.y + m_size.y * 0.5f);

    UpdateChildPositions();
}


bool UIPanel::Picked(POINT _screenPos)
{
    return ::PtInRect(&m_rect, _screenPos);
}

void UIPanel::Create(Vec2 screenPos, Vec2 size, Vec4 diffuseInfo, shared_ptr<Material> backgroundMaterial)
{
    Init();
    m_position = screenPos;
    m_size = size;

    SetPosition(screenPos);

    auto go = GetGameObject();
    go->GetTransform()->SetScale(Vec3(size.x, size.y, 1));

    // 배경 머티리얼 설정
    if (backgroundMaterial) 
    {
        m_backgroundMaterial = backgroundMaterial;
        go->GetMeshRenderer()->SetMaterial(m_backgroundMaterial);
        go->GetMeshRenderer()->SetPass(1);
    }
    else 
    {
        // 기본 배경 머티리얼 생성
        m_backgroundMaterial = make_shared<Material>();
        auto shader = make_shared<Shader>(L"ImageShader.fx");
        m_backgroundMaterial->SetShader(shader);
        m_backgroundMaterial->SetRenderQueue(RenderQueue::Transparent);
        m_backgroundMaterial->SetTransparent(true);
        m_backgroundMaterial->SetRenderingMode(RenderingMode::Forward);

        MaterialDesc& desc = m_backgroundMaterial->GetMaterialDesc();
        desc.ambient = Vec4(0.f, 0.f, 0.f, 1.f);
        desc.diffuse = diffuseInfo;
        desc.specular = Vec4(0.f);
        desc.emissive = Vec4(0.f);

        go->GetMeshRenderer()->SetMaterial(m_backgroundMaterial);
        go->GetMeshRenderer()->SetPass(2);
    }

    //go->GetMeshRenderer()->SetMaterial(m_backgroundMaterial);
    go->SetLayerIndex(LAYER_UI);
}

void UIPanel::OnDestroy()
{
    
    // 자식 요소들 정리만 하면 됨 (Scene에서 알아서 순서대로 소멸)
    for (auto it = m_childElements.begin(); it != m_childElements.end();)
    {
        if (auto child = it->lock())
        {
            // 자식을 Scene에서 삭제 요청
            if (CURSCENE && !CURSCENE->IsDestroying()) {
                CURSCENE->GetObjectManager()->MarkUIObjectForDestroy(child);
            }
            ++it;
        }
        else {
            it = m_childElements.erase(it);
        }
    }

    // 컨테이너들 정리
    m_childElements.clear();
    m_namedElements.clear();

    // 리소스 해제
    if (m_backgroundMaterial) {
        m_backgroundMaterial.reset();
    }
    if (m_backgroundTexture) {
        m_backgroundTexture.reset();
    }
    if (m_backgroundMesh) {
        m_backgroundMesh.reset();
    }

    Super::OnDestroy();

   
}


void UIPanel::SetPosition(const Vec2& position)
{
    m_position = position;

    auto go = GetGameObject();
    if (go) {
        float height = GRAPHICS->GetViewport().GetHeight();
        float width = GRAPHICS->GetViewport().GetWidth();

        float x = position.x - width / 2.0f;
        float y = height / 2.0f - position.y;

        go->GetTransform()->SetPosition(Vec3(x, y, Z_UIPANEL));
    }

    // 자식 요소들의 위치도 업데이트
    UpdateChildPositions();
}

void UIPanel::SetSize(const Vec2& size)
{
    m_size = size;

    auto go = GetGameObject();
    if (go) {
        go->GetTransform()->SetScale(Vec3(size.x, size.y, 1));
    }
}

void UIPanel::SetVisible(bool visible)
{
    m_visible = visible;

    auto go = GetGameObject();
    if (go) {
        // 패널 자체의 가시성 설정
        // 실제 구현에서는 렌더링 활성화/비활성화 처리
        go->SetActive(visible);
    }

    // 자식 요소들의 가시성도 함께 설정 (weak_ptr 사용)
    for (auto& weakChild : m_childElements) {
        if (auto child = weakChild.lock()) {
            // 자식 요소들의 가시성 설정
            if (child->GetUIPanel())
                child->GetUIPanel()->SetVisible(visible);
            else if (child->GetButton())
                child->GetButton()->SetVisible(visible);
            else if (child->GetD2DText())
                child->GetD2DText()->SetVisible(visible);
            else if (child->GetImageUI())
                child->GetImageUI()->SetVisible(visible);
            else if (child->GetSliderUI())
                child->GetSliderUI()->SetVisible(visible);
        }
    }
}

shared_ptr<UIPanel> UIPanel::AddPanel(Vec2 localPos, Vec2 size, shared_ptr<Material> material, const wstring& name)
{
    auto childPanelObj = make_shared<GameObject>();
    childPanelObj->SetName(name);

    auto childPanelComponent = make_shared<UIPanel>();
    childPanelObj->AddComponent(childPanelComponent);
 
    // 월드 좌표로 변환하여 버튼 생성
    Vec2 worldPos = LocalToWorldPosition(localPos);
    childPanelComponent->Create(worldPos, size, Vec4(0.f,0.f,0.f,1.f), material);
    childPanelComponent->SetLocalPosition(localPos);
    // 로컬 위치 저장
    //buttonComponent->SetLocalPosition(localPos);

    // Z 위치를 패널보다 앞쪽으로 설정
    childPanelObj->GetTransform()->SetPosition(Vec3(
        childPanelObj->GetTransform()->GetPosition().x,
        childPanelObj->GetTransform()->GetPosition().y,
        Z_UIPANEL - 0.01  // 패널보다 앞쪽
    ));

    childPanelObj->SetLayerIndex(LAYER_UI);

    // 자식 요소로 등록 (weak_ptr 사용)
    m_childElements.push_back(childPanelObj);
    m_namedElements[name] = childPanelObj;

    // **UI 객체로 씬에 추가 (자식으로 등록)**
    CURSCENE->AddUIObject(childPanelObj, false);  // false = 자식
    CURSCENE->RegisterUIChild(childPanelObj);

    return childPanelComponent;
}

shared_ptr<Button> UIPanel::AddButton(Vec2 localPos, Vec2 size, shared_ptr<Material> material, const wstring& name)
{
    // 버튼 GameObject 생성
    auto buttonObj = make_shared<GameObject>();
    buttonObj->SetName(name);

    // Button 컴포넌트 추가
    auto buttonComponent = make_shared<Button>();
    buttonObj->AddComponent(buttonComponent);
    //buttonComponent->SetParent(GetGameObject());

    // 월드 좌표로 변환하여 버튼 생성
    Vec2 worldPos = LocalToWorldPosition(localPos);
    buttonComponent->Create(worldPos, size, material, 1);

    // 로컬 위치 저장
    buttonComponent->SetLocalPosition(localPos);
     
    // Z 위치를 패널보다 앞쪽으로 설정
    buttonObj->GetTransform()->SetPosition(Vec3(
        buttonObj->GetTransform()->GetPosition().x,
        buttonObj->GetTransform()->GetPosition().y,
        Z_BUTTON  // 패널보다 앞쪽
    ));

    buttonObj->SetLayerIndex(LAYER_UI);

    // 자식 요소로 등록 (weak_ptr 사용)
    m_childElements.push_back(buttonObj);
    m_namedElements[name] = buttonObj;

    // **UI 객체로 씬에 추가 (자식으로 등록)**
    CURSCENE->AddUIObject(buttonObj, false);  // false = 자식
    CURSCENE->RegisterUIChild(buttonObj);

    return buttonComponent;
}

shared_ptr<Text> UIPanel::AddText(Vec2 localPos, const wstring& text, float fontSize,
    Vec4 color, float alpha, Vec4 outlineColor, float outlineWidth, const wstring& name, 
    TextAlignment alignment)
{
    // 텍스트 GameObject 생성
    auto textObj = make_shared<GameObject>();
    textObj->SetName(name);

    // Text 컴포넌트 추가
    auto textComponent = make_shared<Text>();
    textObj->AddComponent(textComponent);

    // 월드 좌표로 변환하여 텍스트 생성
    Vec2 worldPos = LocalToWorldPosition(localPos);
   
    textComponent->Create(worldPos, text, fontSize, color, alpha, outlineColor, outlineWidth, alignment);

    // 로컬 위치 저장
    textComponent->SetLocalPosition(localPos);

    // Z 위치를 패널보다 앞쪽으로 설정
    textObj->GetTransform()->SetPosition(Vec3(
        textObj->GetTransform()->GetPosition().x,
        textObj->GetTransform()->GetPosition().y,
        Z_TEXT  // 버튼보다도 앞쪽
    ));

    textObj->SetLayerIndex(LAYER_UI);

    // 자식 요소로 등록 (weak_ptr 사용)
    m_childElements.push_back(textObj);
    m_namedElements[name] = textObj;

    // **UI 객체로 씬에 추가 (자식으로 등록)**
    CURSCENE->AddUIObject(textObj, false);  // false = 자식
    CURSCENE->RegisterUIChild(textObj);

    return textComponent;
}

shared_ptr<ImageUI> UIPanel::AddImageUI(Vec2 localPos, const wstring& name)
{
    // ImageUI GameObject 생성
    auto imageUIObj = make_shared<GameObject>();
    imageUIObj->SetName(name);

    // ImageUI 컴포넌트 추가
    auto imageUIComponent = make_shared<ImageUI>();
    imageUIObj->AddComponent(imageUIComponent);

    // 위치 설정
    Vec2 worldPos = LocalToWorldPosition(localPos);
   
    /*float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();
    float x = worldPos.x - width / 2.0f;
    float y = height / 2.0f - worldPos.y;*/

    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();
    float x = worldPos.x;
    float y = worldPos.y;

    imageUIComponent->SetLocalPosition(localPos);
    
    imageUIObj->GetTransform()->SetPosition(Vec3(x, y, Z_IMAGEUI_BASE));
    imageUIObj->SetLayerIndex(LAYER_UI);

    // 자식 요소로 등록 (weak_ptr 사용)
    m_childElements.push_back(imageUIObj);
    m_namedElements[name] = imageUIObj;

    // **UI 객체로 씬에 추가 (자식으로 등록)**
    CURSCENE->AddUIObject(imageUIObj, false);  // false = 자식
    CURSCENE->RegisterUIChild(imageUIObj);

    return imageUIComponent;
}

shared_ptr<SliderUI> UIPanel::AddSliderUI(Vec2 localPos, Vec2 size, shared_ptr<Material> trackMaterial, shared_ptr<Material> fillMaterial, shared_ptr<Material> handleMaterial, float minValue, float maxValue, const wstring& name)
{
    auto sliderObj = make_shared<GameObject>();
    sliderObj->SetName(name);

    auto sliderComponent = make_shared<SliderUI>();
    sliderObj->AddComponent(sliderComponent);

    Vec2 worldPos = LocalToWorldPosition(localPos);
    sliderComponent->Create(worldPos, size, trackMaterial, fillMaterial, handleMaterial, minValue, maxValue);
    sliderComponent->SetLocalPosition(localPos);

    // 자식 요소로 등록
    m_childElements.push_back(sliderObj);
    m_namedElements[name] = sliderObj;

    CURSCENE->AddUIObject(sliderObj, false);
    CURSCENE->RegisterUIChild(sliderObj);

    return sliderComponent;
}

void UIPanel::RemoveUIElement(const wstring& name)
{
    auto it = m_namedElements.find(name);
    if (it != m_namedElements.end()) {
        if (auto child = it->second.lock()) {
            // 벡터에서 제거
            auto vecIt = std::find_if(m_childElements.begin(), m_childElements.end(),
                [&child](const weak_ptr<GameObject>& weakPtr) {
                    return !weakPtr.owner_before(child) && !child.owner_before(weakPtr);
                });

            if (vecIt != m_childElements.end()) {
                m_childElements.erase(vecIt);
            }

            // 지연 삭제 시스템 사용
            if (CURSCENE && !CURSCENE->IsDestroying()) {
                CURSCENE->GetObjectManager()->MarkUIObjectForDestroy(child);
            }
        }
        m_namedElements.erase(it);
    }
}

shared_ptr<UIPanel> UIPanel::GetChildUIPanel(const wstring& name)
{
    auto it = m_namedElements.find(name);
    if (it != m_namedElements.end()) {
        if (auto child = it->second.lock()) {
            return child->GetUIPanel();
        }
    }
    return nullptr;
}

shared_ptr<Button> UIPanel::GetButton(const wstring& name)
{
    auto it = m_namedElements.find(name);
    if (it != m_namedElements.end()) {
        if (auto child = it->second.lock()) {
            return child->GetButton();
        }
    }
    return nullptr;
}

shared_ptr<Text> UIPanel::GetText(const wstring& name)
{
    auto it = m_namedElements.find(name);
    if (it != m_namedElements.end()) {
        if (auto child = it->second.lock()) {
            return child->GetText();
        }
    }
    return nullptr;
}

shared_ptr<ImageUI> UIPanel::GetImageUI(const wstring& name)
{
    auto it = m_namedElements.find(name);
    if (it != m_namedElements.end()) {
        if (auto child = it->second.lock()) {
            return child->GetImageUI();
        }
    }
    return nullptr;
}

shared_ptr<SliderUI> UIPanel::GetSliderUI(const wstring& name)
{
    auto it = m_namedElements.find(name);
    if (it != m_namedElements.end()) {
        if (auto child = it->second.lock()) {
            return child->GetSliderUI();
        }
    }
    return nullptr;
}

// UIPanel.cpp에 구현
void UIPanel::RemoveUIElementSafely(const wstring& name)
{
    if (m_isDestroying) return;

    auto it = m_namedElements.find(name);
    if (it != m_namedElements.end()) {
        if (auto child = it->second.lock()) {
            // 계층적 삭제 사용
            if (CURSCENE && !CURSCENE->IsDestroying()) {
                CURSCENE->GetObjectManager()->MarkUIObjectForDestroyWithChildren(child);
            }

            // 벡터에서 제거
            auto vecIt = std::find_if(m_childElements.begin(), m_childElements.end(),
                [&child](const weak_ptr<GameObject>& weakPtr) {
                    return !weakPtr.owner_before(child) && !child.owner_before(weakPtr);
                });

            if (vecIt != m_childElements.end()) {
                m_childElements.erase(vecIt);
            }
        }
        m_namedElements.erase(it);
    }
}

void UIPanel::CreatePanelBackground()
{
    // Quad 메시 생성
    m_backgroundMesh = make_shared<Mesh>();
   // m_backgroundMesh->CreateQuad();
    m_backgroundMesh = RESOURCES->Get<Mesh>(L"Quad");

    auto go = GetGameObject();
    go->GetMeshRenderer()->SetMesh(m_backgroundMesh);
    //go->GetMeshRenderer()->SetPass(1);
}

void UIPanel::UpdateChildPositions()
{
    // 패널 위치가 변경되면 자식 요소들의 위치도 업데이트
    // weak_ptr을 사용하여 안전하게 접근
    Vec2 panelLeftTop = Vec2(m_position.x - m_size.x / 2.f
        , m_position.y - m_size.y / 2.f);


    for (auto it = m_childElements.begin(); it != m_childElements.end();) {
        if (auto child = it->lock()) {
            // 자식 위치 업데이트 로직
            if (auto button = child->GetButton()) {
                button->UpdatePosition(panelLeftTop);
            }
            else if (auto text = child->GetText()) {
                text->UpdatePosition(panelLeftTop);
            }
            else if (auto ImageUI = child->GetImageUI())
            {
                ImageUI->UpdatePosition(panelLeftTop);
            }
            else if (auto d2dText = child->GetD2DText()) {
                d2dText->UpdatePosition(panelLeftTop);
            }
            else if (auto uiPanel = child->GetUIPanel()) {
                uiPanel->UpdatePosition(panelLeftTop);
            }

            ++it;
        }
        else {
            // 만료된 weak_ptr 제거
            it = m_childElements.erase(it);
        }
    }
}

void UIPanel::UpdatePosition(const Vec2& parentWorldPos)
{
    auto go = m_gameObject.lock();
    if (!go) return;

    Vec2 newWorldPos;
    newWorldPos.x = parentWorldPos.x + m_localPosition.x;
    newWorldPos.y = parentWorldPos.y + m_localPosition.y;

    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();

    float x = newWorldPos.x - width / 2;
    float y = height / 2 - newWorldPos.y;

    go->GetTransform()->SetPosition(Vec3(x, y, go->GetTransform()->GetPosition().z));
}

Vec2 UIPanel::LocalToWorldPosition(const Vec2& localPos)
{
    // 패널 내의 로컬 좌표를 월드 화면 좌표로 변환
    Vec2 worldPos;
    worldPos.x = m_position.x + localPos.x - (m_size.x / 2.0f);
    worldPos.y = m_position.y + localPos.y - (m_size.y / 2.0f);
    return worldPos;
}


shared_ptr<D2DText> UIPanel::AddD2DText(Vec2 localPos, const wstring& text, float fontSize,
    Vec4 color, float alpha, Vec4 outlineColor, float outlineWidth, const wstring& name,
    TextAlignment alignment)
{
    // D2DText GameObject 생성
    auto d2dTextObj = make_shared<GameObject>();
    d2dTextObj->SetName(name);

    // D2DText 컴포넌트 추가
    auto d2dTextComponent = make_shared<D2DText>();
    d2dTextObj->AddComponent(d2dTextComponent);

    // 월드 좌표로 변환하여 텍스트 생성
    Vec2 worldPos = LocalToWorldPosition(localPos);

    d2dTextComponent->Create(worldPos, text, fontSize, color, alpha, outlineColor, outlineWidth, alignment);

    // 로컬 위치 저장
    d2dTextComponent->SetLocalPosition(localPos);

    // Z 위치를 패널보다 앞쪽으로 설정
    d2dTextObj->GetTransform()->SetPosition(Vec3(
        d2dTextObj->GetTransform()->GetPosition().x,
        d2dTextObj->GetTransform()->GetPosition().y,
        Z_TEXT  // 버튼보다도 앞쪽 (기존 Text와 같은 깊이)
    ));

    d2dTextObj->SetLayerIndex(LAYER_UI);

    // 자식 요소로 등록 (weak_ptr 사용)
    m_childElements.push_back(d2dTextObj);
    m_namedElements[name] = d2dTextObj;

    // **UI 객체로 씬에 추가 (자식으로 등록)**
    CURSCENE->AddUIObject(d2dTextObj, false);  // false = 자식
    CURSCENE->RegisterUIChild(d2dTextObj);

    return d2dTextComponent;
}

shared_ptr<D2DText> UIPanel::GetD2DText(const wstring& name)
{
    auto it = m_namedElements.find(name);
    if (it != m_namedElements.end()) {
        if (auto child = it->second.lock()) {
            // D2DText는 Text 컴포넌트 슬롯을 사용하므로 동일하게 처리
           // return dynamic_pointer_cast<D2DText>(child->GetFixedComponent(ComponentType::D2DText));
            return child->GetD2DText();
        }
    }
    return nullptr;
}

void UIPanel::SetBackgroundMaterial(shared_ptr<Material> material)
{
    m_backgroundMaterial = material;
    GetGameObject()->GetMeshRenderer()->SetMaterial(m_backgroundMaterial);
    GetGameObject()->GetMeshRenderer()->SetPass(1);
}

void UIPanel::HandleDragInput()
{
    if (!m_isDraggable) return;

    POINT mousePos = INPUT->GetMousePos();
    Vec2 currentMousePos = Vec2(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));

    // 마우스 왼쪽 버튼이 눌렸을 때
    if (INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
    {
        // 패널 영역 내에서 클릭했는지 확인
        if (Picked(mousePos))
        {
            StartDrag(currentMousePos);
        }
    }
    // 드래그 중일 때
    else if (INPUT->GetButton(KEY_TYPE::LBUTTON) && m_isDragging)
    {
        UpdateDrag(currentMousePos);
    }
    // 마우스 버튼을 뗐을 때
    else if (INPUT->GetButtonUp(KEY_TYPE::LBUTTON) && m_isDragging)
    {
        EndDrag();
    }
}

void UIPanel::StartDrag(Vec2 mousePos)
{
    m_isDragging = true;
    m_dragStartPos = mousePos;
    m_panelStartPos = m_position;
}

void UIPanel::UpdateDrag(Vec2 mousePos)
{
    if (!m_isDragging) return;

    // 마우스 이동량 계산
    Vec2 mouseDelta = mousePos - m_dragStartPos;

    // 새로운 패널 위치 계산
    Vec2 newPosition = m_panelStartPos + mouseDelta;

    // 화면 경계 제한 (선택사항)
    float viewportWidth = GRAPHICS->GetViewport().GetWidth();
    float viewportHeight = GRAPHICS->GetViewport().GetHeight();

    // 패널이 화면 밖으로 나가지 않도록 제한
    float halfWidth = m_size.x * 0.5f;
    float halfHeight = m_size.y * 0.5f;

    newPosition.x = max(halfWidth, min(viewportWidth - halfWidth, newPosition.x));
    newPosition.y = max(halfHeight, min(viewportHeight - halfHeight, newPosition.y));

    // 위치 업데이트
    SetPosition(newPosition);
    m_position = newPosition;

    UpdateChildPositions();
}

void UIPanel::EndDrag()
{
    m_isDragging = false;
}
void UIPanel::SetBackgroundColor(const Vec4& color)
{
    if (m_backgroundMaterial) m_backgroundMaterial->GetMaterialDesc().diffuse = color;
}
