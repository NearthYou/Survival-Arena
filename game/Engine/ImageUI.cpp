#include "pch.h"
#include "ImageUI.h"
#include "Transform.h"
#include "Material.h"
#include "Mesh.h"
#include "Texture.h"
#include "MeshRenderer.h"
#include "Shader.h"
#include "GameObject.h"
#include "Scene.h"
#include "SceneObjectManager.h"

ImageUI::ImageUI() : Super(ComponentType::Image), m_needsSort(false), m_isDestroying(false)
{
}

ImageUI::~ImageUI()
{
    m_isDestroying = true;
    ClearAllLayers();
}

void ImageUI::ClearAllLayers()
{
    if (m_isDestroying) return;

    for (auto& pair : m_imageLayers)
    {
        if (pair.second.gameObject)
        {
            // Scene이 유효하고 소멸 중이 아닐 때만 Remove
            if (CURSCENE)
            {
                // UI 객체 컨테이너에서 제거
                auto& uiObjects = CURSCENE->GetUIObjects();


                auto it = uiObjects.find(pair.second.gameObject);
                if (it != uiObjects.end()) {
                    uiObjects.erase(it);
                }

                // UI 자식 컨테이너에서도 제거
                auto& uiChildren = CURSCENE->GetUIChildren();
                auto childIt = std::find(uiChildren.begin(), uiChildren.end(), pair.second.gameObject);
                if (childIt != uiChildren.end()) {
                    uiChildren.erase(childIt);
                }
            }
            pair.second.gameObject = nullptr;
        }
    }
    m_imageLayers.clear();
    m_sortedLayers.clear();
}

void ImageUI::SetVisible(bool visible)
{
    m_visible = visible;
    GetGameObject()->SetActive(visible);

    for (auto layer : m_imageLayers)
    {
        layer.second.gameObject->SetActive(visible);
    }
}

void ImageUI::OnDestroy()
{
    m_isDestroying = true;

    // 모든 레이어 객체들을 지연 삭제로 처리
    for (auto& pair : m_imageLayers)
    {
        if (pair.second.gameObject)
        {
            // Scene의 지연 삭제 시스템 사용
            if (CURSCENE && !CURSCENE->IsDestroying()) {
                CURSCENE->GetObjectManager()->MarkUIObjectForDestroy(pair.second.gameObject);
            }
            pair.second.gameObject = nullptr;
        }
    }

    // 레이어 정보 정리
    m_imageLayers.clear();
    m_sortedLayers.clear();

    Super::OnDestroy();
}

void ImageUI::AddImageLayer(int layer, Vec2 screenPos, Vec2 size,
    shared_ptr<Material> material, uint32 pass)
{
    if (m_isDestroying) return;

    ImageLayer imageLayer;
    imageLayer.layer = layer;
    imageLayer.position = screenPos;
    imageLayer.size = size;
    imageLayer.material = material;
    imageLayer.pass = pass;

    // 기존 레이어가 있다면 제거
    if (m_imageLayers.find(layer) != m_imageLayers.end())
    {
        RemoveImageLayer(layer);
    }

    // 새 레이어 추가
    m_imageLayers[layer] = imageLayer;
    CreateImageGameObject(m_imageLayers[layer]);

    m_needsSort = true;
}

void ImageUI::RemoveImageLayer(int layer)
{
    auto it = m_imageLayers.find(layer);
    if (it != m_imageLayers.end())
    {
        if (it->second.gameObject && CURSCENE && !m_isDestroying)
        {
            // 지연 삭제 시스템 사용
            CURSCENE->GetObjectManager()->MarkUIObjectForDestroy(it->second.gameObject);
        }
        m_imageLayers.erase(it);
        m_needsSort = true;
    }
}
void ImageUI::SetLayerOrder(int layer, int newOrder)
{
    auto it = m_imageLayers.find(layer);
    if (it != m_imageLayers.end())
    {
        // 기존 레이어 데이터 백업
        ImageLayer temp = it->second;

        // 기존 레이어 제거
        m_imageLayers.erase(it);

        // 새 순서로 다시 추가
        temp.layer = newOrder;
        m_imageLayers[newOrder] = temp;

        // GameObject의 Transform 업데이트 (Z 좌표 변경)
        UpdateImageGameObject(m_imageLayers[newOrder]);

        m_needsSort = true;
    }
}

void ImageUI::SetLayerPosition(int layer, Vec2 newPosition)
{
    auto it = m_imageLayers.find(layer);
    if (it != m_imageLayers.end())
    {
        it->second.position = newPosition;
        UpdateImageGameObject(it->second);
    }
}

void ImageUI::SetLayerSize(int layer, Vec2 newSize)
{
    auto it = m_imageLayers.find(layer);
    if (it != m_imageLayers.end())
    {
        it->second.size = newSize;
        UpdateImageGameObject(it->second);
    }
}

void ImageUI::SetMaterial(int layer, shared_ptr<Material> material)
{
    // 레이어가 존재하는지 확인
    auto it = m_imageLayers.find(layer);
    if (it != m_imageLayers.end()) {
        it->second.material = material;

        // GameObject의 MeshRenderer도 업데이트
        if (it->second.gameObject && it->second.gameObject->GetMeshRenderer()) {
            it->second.gameObject->GetMeshRenderer()->SetMaterial(material);
        }
    }
    else {
        std::wcout << L"Layer " << layer << L" not found in ImageUI!" << std::endl;
    }
}

void ImageUI::CreateImageGameObject(ImageLayer& layer)
{
    if (m_isDestroying) return;

    auto go = GetGameObject();
    Vec3 parentPos = go->GetTransform()->GetPosition();

   
    // GameObject 생성
    layer.parentPos = Vec2(parentPos.x, parentPos.y);
    layer.gameObject = make_shared<GameObject>();
    layer.gameObject->SetName(L"ImageLayer_" + std::to_wstring(layer.layer));

    // Transform 설정
    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();

    /*float x = parentPos.x + layer.position.x - width / 2;
    float y = parentPos.y + height / 2 - layer.position.y;*/

    float x = parentPos.x + layer.position.x - width / 2;
    float y = height / 2 - (parentPos.y + layer.position.y);

    // 레이어 순서에 따른 Z값 계산
    float z = m_zPos - (static_cast<float>(layer.layer) * 0.001f);

    Vec3 position = Vec3(x, y, z);
    layer.gameObject->GetTransform()->SetPosition(position);
    layer.gameObject->GetTransform()->SetScale(Vec3(layer.size.x * RESOLUTION_CONSTANT, layer.size.y * RESOLUTION_CONSTANT, 1));

    // 컴포넌트 설정
    layer.gameObject->SetLayerIndex(LAYER_UI);
    layer.gameObject->AddComponent(make_shared<MeshRenderer>());
    layer.gameObject->GetMeshRenderer()->SetMaterial(layer.material);

    auto mesh = RESOURCES->Get<Mesh>(L"Quad");
    layer.gameObject->GetMeshRenderer()->SetMesh(mesh);
    layer.gameObject->GetMeshRenderer()->SetPass(layer.pass);

    // **UI 객체로 씬에 추가 (자식으로 등록)**
    if (CURSCENE)
    {
        CURSCENE->AddUIObject(layer.gameObject, false);  // false = 자식
        CURSCENE->RegisterUIChild(layer.gameObject);
    }
}

void ImageUI::UpdateImageGameObject(ImageLayer& layer)
{
    if (!layer.gameObject || m_isDestroying) return;

   
    // Transform 업데이트
    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();

    float x = layer.parentPos.x + layer.position.x - width / 2;
    float y = height / 2 - (layer.position.y  + layer.parentPos.y );

 
    // 기존 Z좌표 보존
    Vec3 currentPos = layer.gameObject->GetTransform()->GetPosition();
    Vec3 position = Vec3(x, y, currentPos.z);  // Z좌표 보존

    
    layer.gameObject->GetTransform()->SetPosition(position);
    layer.gameObject->GetTransform()->SetScale(Vec3(layer.size.x, layer.size.y, 1));
}

void ImageUI::SortLayersByOrder()
{
    if (!m_needsSort || m_isDestroying) return;

    m_sortedLayers.clear();
    for (const auto& pair : m_imageLayers)
    {
        m_sortedLayers.push_back(pair.first);
    }

    // 오름차순 정렬 (낮은 레이어 번호가 뒤쪽, 높은 번호가 앞쪽)
    std::sort(m_sortedLayers.begin(), m_sortedLayers.end());

    // Z 좌표를 레이어 순서에 따라 설정
    for (size_t i = 0; i < m_sortedLayers.size(); ++i)
    {
        int layer = m_sortedLayers[i];
        auto& imageLayer = m_imageLayers[layer];
        if (imageLayer.gameObject)
        {
            Vec3 pos = imageLayer.gameObject->GetTransform()->GetPosition();
            // 레이어 번호가 낮을수록 뒤쪽(더 작은 Z값)
            pos.z = m_zPos - (static_cast<float>(layer) * 0.001f);
            imageLayer.gameObject->GetTransform()->SetPosition(pos);
        }
    }

    m_needsSort = false;
}

void ImageUI::UpdateLayers()
{
    if (m_isDestroying) return;

    SortLayersByOrder();

    // 모든 레이어의 GameObject 업데이트
    for (auto& pair : m_imageLayers)
    {
        UpdateImageGameObject(pair.second);
    }
}

Vec2 ImageUI::GetLayerPosition(int layer)
{
    auto it = m_imageLayers.find(layer);
    if (it != m_imageLayers.end())
    {
        return it->second.position;
    }
    else
        return (Vec2(-1, -1));
}

void ImageUI::Update()
{
    if (!m_isDestroying && m_needsSort)
    {
        SortLayersByOrder();
    }
    // UpdateLayers() 호출 제거 - 필요할 때만 호출
}

// Button.cpp에 새로운 함수들 추가
void ImageUI::UpdatePosition(const Vec2& parentWorldPos)
{
    auto go = m_gameObject.lock();
    if (!go) return;

    // 부모의 월드 위치 + 로컬 위치로 새 월드 위치 계산
    Vec2 newWorldPos;
    newWorldPos.x = parentWorldPos.x + m_localPosition.x;
    newWorldPos.y = parentWorldPos.y - m_localPosition.y;

    // 화면 좌표를 월드 좌표로 변환
    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();

    float x = newWorldPos.x - width / 2;
    float y = height / 2 - newWorldPos.y;

    go->GetTransform()->SetPosition(Vec3(x, y, m_zPos));

    map<int, ImageLayer>::iterator iter = m_imageLayers.begin();
    for (; iter != m_imageLayers.end(); iter++)
    {
        auto layerGameObject = iter->second.gameObject;
        Vec3 imageUIPos = go->GetTransform()->GetPosition();

        layerGameObject->GetTransform()->SetPosition(
            Vec3(imageUIPos.x + iter->second.position.x
                , imageUIPos.y - iter->second.position.y
                , m_zPos - (static_cast<float>(iter->second.layer) * 0.001f)));
    }
}