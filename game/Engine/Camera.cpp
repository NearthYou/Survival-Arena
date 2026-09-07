#include "pch.h"
#include "Camera.h"
#include "Component.h"
#include "Transform.h"
#include "Scene.h"
#include "Renderer.h"
#include "Material.h"
#include "QuadTree.h"
#include "GameObject.h"
#include "IFogOfWar.h"
#include "BaseCollider.h"
#include "MonoBehaviour.h"


Matrix Camera::s_MatView = Matrix::Identity;
Matrix Camera::s_MatProjection = Matrix::Identity;
Vec3 Camera::s_Pos = Vec3::Zero;

Camera::Camera() : Super(ComponentType::Camera)
{
	m_width = static_cast<float>(GAME->GetGameDesc().width);
	m_height = static_cast<float>(GAME->GetGameDesc().height);
}

Camera::~Camera()
{

}

void Camera::LateUpdate()
{
	UpdateMatrix();
}


//카메라랑 연관있는 건 View와 Projection.

//View는 당연히 카메라 좌표 기준에서 봐야하니까.
//Projection은 
void Camera::UpdateMatrix()
{
	Vec3 eyePosition = GetTransform()->GetPosition();
	Vec3 focusPosition = eyePosition + GetTransform()->GetLook();
	Vec3 upDirection = GetTransform()->GetUp();
	
	m_matView = ::XMMatrixLookAtLH(eyePosition, focusPosition, upDirection);

	if (m_type == ProjectionType::Perspective) 
	{
		m_matProjection = s_MatProjection = ::XMMatrixPerspectiveFovLH(m_fov, m_width / m_height, m_near, m_far);
	}
	else 
	{
		m_matProjection = s_MatProjection = ::XMMatrixOrthographicLH(m_width, m_height, m_near, m_far);
	}
}

void Camera::SortGameObject()
{
    if (m_type == ProjectionType::Perspective)
    {
        SortGameObjects();
    }
    else
    {
        SortUIObjects();
    }
}

void Camera::SortGameObjects()
{
    shared_ptr<Scene> scene = CURSCENE;
    const unordered_set<shared_ptr<GameObject>>& gameObjects = scene->GetObjects();

    m_vecForward.clear();
    m_vecBackward.clear();

    // FOW 캐싱 (성능 최적화)
    static IFogOfWar* cachedFogOfWar = nullptr;
    static int lastFrameCheck = -1;
    int currentFrame = GetTickCount64() / 16; // 60FPS 기준

    if (lastFrameCheck != currentFrame) {
        cachedFogOfWar = nullptr;

        // IFogOfWar 인터페이스 구현체 찾기
        for (auto& obj : gameObjects) {
            auto& scripts = obj->GetScripts();
            for (auto& comp : scripts) {
                IFogOfWar* fogInterface = dynamic_cast<IFogOfWar*>(comp.get());
                if (fogInterface) {
                    cachedFogOfWar = fogInterface;
                    break;
                }
            }
            if (cachedFogOfWar) break;
        }
        lastFrameCheck = currentFrame;
    }

    // FOW 시스템 업데이트 (엔진에서 호출)
    if (cachedFogOfWar) {
        cachedFogOfWar->UpdateFOWSystem();
    }


    //그려줄 것 선별하기. 
    for (auto& object : gameObjects)
    {
        if (object->GetActive() == false)
            continue;

        if (object->GetType() != OBJECTTYPE::MAP) {
            //레이어 컬링. 
            if (IsCulled(object->GetLayerIndex()))
                continue;

            // QuadTree를 통한 Frustum Culling.
            if (!scene->GetQuadTree()->IsObjectVisible(object, this))
                continue;


            //FOW통한 컬링. 
            if (cachedFogOfWar) {
                if (!cachedFogOfWar->ShouldRenderObject(object)) {
                    continue;
                }
            }

            //Collider중에 안 보일 것 걸러내기.
            if (object->GetMeshRenderer() != nullptr) {
                //Collider는 무조건 MeshRenderer
                if (!object->GetColliderActive())
                    continue;
            }

            /*if (object->GetCollider()) {
                if (object->GetCollider()->GetVisible() == false)
                    continue;
            }*/
        }


        //QuadTree - Visible가지고 Frustum Culling 가능. 
        shared_ptr<Renderer> renderer = object->GetRenderer();
        if (renderer == nullptr)
            continue;

        RenderQueue renderQueue = renderer->GetMaterial()->GetRenderQueue();

        switch (renderQueue)
        {
        case RenderQueue::Opaque:
        case RenderQueue::Cutout:
            m_vecForward.push_back(object);
            break;
        case RenderQueue::Transparent:
            m_vecBackward.push_back(object);
            break;

        }
    }
}

void Camera::SortUIObjects()
{
    //// UI 전용 깊이 스텐실 상태 생성 및 적용
    //D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    //dsDesc.DepthEnable = true;                    // 깊이 테스트 비활성화
    //dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;  // 깊이 쓰기 비활성화
    //dsDesc.StencilEnable = true;

    //ComPtr<ID3D11DepthStencilState> uiDepthState;
    //GRAPHICS->GetDevice()->CreateDepthStencilState(&dsDesc, uiDepthState.GetAddressOf());

    //// UI 렌더링 전에 적용
    //GRAPHICS->GetDeviceContext()->OMSetDepthStencilState(uiDepthState.Get(), 0);

    ///// UI 렌더링 전에 알파 블렌딩 상태 강제 설정
    //D3D11_BLEND_DESC blendDesc = {};
    //blendDesc.AlphaToCoverageEnable = false;
    //blendDesc.IndependentBlendEnable = false;
    //blendDesc.RenderTarget[0].BlendEnable = true;
    //blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    //blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    //blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    //blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    //blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    //blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    //blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    //ComPtr<ID3D11BlendState> alphaBlendState;
    //GRAPHICS->GetDevice()->CreateBlendState(&blendDesc, alphaBlendState.GetAddressOf());

    //float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    //GRAPHICS->GetDeviceContext()->OMSetBlendState(alphaBlendState.Get(), blendFactor, 0xFFFFFFFF);

    shared_ptr<Scene> scene = CURSCENE;

    const unordered_set<shared_ptr<GameObject>>& uiObjects = scene->GetUIObjects();

    m_vecForward.clear();
    m_vecBackward.clear();

    vector<shared_ptr<GameObject>> sortedUIObjects(uiObjects.begin(), uiObjects.end());

    std::sort(sortedUIObjects.begin(), sortedUIObjects.end(),
        [](const shared_ptr<GameObject>& a, const shared_ptr<GameObject>& b) {
            return a->GetTransform()->GetPosition().z > b->GetTransform()->GetPosition().z;
        });

    for (auto& object : sortedUIObjects) {
        if (object->GetActive() == false)
        {
            continue;
        }

        if (IsCulled(object->GetLayerIndex()))
            continue;

        shared_ptr<Renderer> renderer = object->GetRenderer();
        if (renderer == nullptr)
            continue;

        RenderQueue renderQueue = renderer->GetMaterial()->GetRenderQueue();

        switch (renderQueue) {
        case RenderQueue::Opaque:
        case RenderQueue::Cutout:
            m_vecForward.push_back(object);
            break;
        case RenderQueue::Transparent:
            m_vecBackward.push_back(object);
            break;
        }
    }
}


void Camera::SetStaticData() {
	s_MatView = m_matView;
	s_MatProjection = m_matProjection;
	s_Pos = GetTransform()->GetPosition();
}

void Camera::Render_Forward(bool _isShadowTech)
{
	RENDER->RenderForward(m_vecForward, _isShadowTech);
}

void Camera::Render_Backward(bool _isShadowTech)
{
	RENDER->RenderForward(m_vecBackward, _isShadowTech);
}
