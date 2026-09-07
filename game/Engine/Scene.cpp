#include "pch.h"
#include "Scene.h"
#include <iostream>
#include "GameObject.h"
#include "BaseCollider.h"
#include "Camera.h"
#include "Button.h"
#include "Sky.h"
#include "Light.h"
#include "Terrain.h"
#include "QuadTree.h"
#include "SphereCollider.h"
#include "AABBBoxCollider.h"
#include "UIPanel.h"
#include "SceneObjectManager.h"

Scene::Scene()
{
    m_objectManager = make_unique<SceneObjectManager>();
}

Scene::~Scene()
{
   
}

void Scene::RegisterUIParent(shared_ptr<GameObject> parent)
{
    m_objectManager->RegisterUIParent(parent);
}

void Scene::RegisterUIChild(shared_ptr<GameObject> child)
{
    m_objectManager->RegisterUIChild(child);
}

void Scene::Start()
{
    //충돌 판정 초기화. 
    m_mapColInfo.clear();
    m_objectManager->Start();
    m_objectManager->UpdateQuadTree();
}

void Scene::Update()
{
    m_objectManager->Update();

    m_objectManager->UpdateQuadTree();

    if(INPUT->GetButtonDown(KEY_TYPE::LBUTTON) != false)
        m_pickedObject = m_objectManager->PickObjectOrUI(); 
    //이 밑에다가 디버그용 
#if _DEBUG
    GUI->ShowPickedObj();
#endif
}

void Scene::FixedUpdate()
{
    m_objectManager->FixedUpdate();
}

void Scene::LateUpdate()
{

    //QuadTree방식. 
    CheckCollisionWithQuadTree();


    m_objectManager->LateUpdate();

    // start = std::chrono::high_resolution_clock::now();

    //auto end = std::chrono::high_resolution_clock::now();
    //auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    //cout << "BruteForce 걸리는 시간 :" << duration.count() << "us\n";

    // 지연 삭제 처리 추가
    m_objectManager->ProcessPendingDestroy();
}

void Scene::Render()
{
    /*const auto& cameras = m_objectManager->GetCameras();
    for (auto camera : cameras) {

        Camera* cam = camera->GetCamera().get();
        if (cam->GetProjectionType() == ProjectionType::Perspective) {
            RenderGameCamera(cam);
        }
        else {
            RenderUICamera(cam);
        }
    }*/

    auto main_camera = m_objectManager->GetMainCamera();
    Camera* cam = main_camera->GetCamera().get();
    RenderGameCamera(cam);

    auto ui_Camera = m_objectManager->GetUICamera();
    cam = ui_Camera->GetCamera().get();
    RenderUICamera(cam);
}

void Scene::RenderGameCamera(Camera* cam)
{
    GRAPHICS->ClearShadowDepthStencilView();
    GRAPHICS->SetShadowDepthStencilView();

    Light* light = GetLight()->GetLight().get();

    cam->SetStaticData();
    cam->SortGameObject();

    if (light) {
        light->SetVPMatrix(cam, 200.0f, ::XMMatrixOrthographicLH(300.f, 300.f, 1.f, 500.f));
        cam->Render_Forward(true);
        Viewport& vp = GRAPHICS->GetShadowViewport();
        cam->Render_Backward(true);
    }

    GRAPHICS->SetRTVAndDSV();
    //GRAPHICS->ClearDepthStencilView(); // 이 줄 추가

    cam->SetStaticData();


    //if (GetQuadTree() == nullptr) return;
    //if (GetQuadTree()->GetInsertedObject().empty()) return;
    //vector<shared_ptr<GameObject>> objects(GetQuadTree()->GetInsertedObject().begin(), GetQuadTree()->GetInsertedObject().end());
    static vector<shared_ptr<GameObject>> combined;
    combined.clear();
    vector<shared_ptr<GameObject>> forward = cam->GetForwardObjects();
    vector<shared_ptr<GameObject>> backward = cam->GetBackwardObjects();
    combined.reserve(forward.size() + backward.size());

    if (m_objectManager->m_sky)
        m_objectManager->m_sky->Render(cam);
    combined.insert(combined.end(), forward.begin(), forward.end());
    combined.insert(combined.end(), backward.begin(), backward.end());


    
    RENDER->Render(combined, false);
    //RENDER->Render(cam->GetBackwardObjects(), false);
    /*cam->Render_Forward(false);
    if (m_objectManager->m_sky)
        m_objectManager->m_sky->Render(cam);
    cam->Render_Backward(false);*/
}

void Scene::RenderUICamera(Camera* cam)
{
    GRAPHICS->ClearDepthStencilView();

    cam->SetStaticData();
    cam->SortGameObject();
    cam->Render_Forward(false);
    cam->Render_Backward(false);
}

void Scene::Add(shared_ptr<GameObject> _object)
{
    m_objectManager->Add(_object);
}

void Scene::AddUIObject(shared_ptr<GameObject> _object, bool isParent)
{
    m_objectManager->AddUIObject(_object, isParent);
}

void Scene::Remove(shared_ptr<GameObject> _object)
{
    // UI 객체의 경우 계층적 삭제 사용
    if (m_objectManager->GetUIObjects().find(_object) != m_objectManager->GetUIObjects().end()) {
        m_objectManager->MarkUIObjectForDestroyWithChildren(_object);
    }
    else {
        m_objectManager->Remove(_object);
    }
}

shared_ptr<GameObject> Scene::GetMainCamera()
{
    return m_objectManager->GetMainCamera();
}

shared_ptr<GameObject> Scene::GetUICamera()
{
    return m_objectManager->GetUICamera();
}


// Brute-force collision baseline.
void Scene::CheckCollision()
{
    //1. m_gameObjects끼리 for문 돌려서 검사한다. 
    //2. Collider만들 때, obj를 받는다. 
    vector<shared_ptr<BaseCollider>> colliders;
    const auto& objects = m_objectManager->GetObjects();
    for (auto& object : objects) {
        if (object->GetCollider() == nullptr)
            continue;

        colliders.push_back(object->GetCollider());
    }

    //BruteForce
    //쿼드 트리 같은 거. 

    //Collider끼리 검사. -> GameObject에 Collision처리 해야하는데. 
    for (uint32 idx = 0; idx < colliders.size(); ++idx) {
        if (colliders[idx].get()->GetActive() == false)
            continue;

        for (uint32 jdx = idx + 1; jdx < colliders.size(); ++jdx) {
            shared_ptr<BaseCollider>& other = colliders[jdx];

            if (other.get()->GetActive() == false)
                continue;


            COLLIDER_ID id;
            id.left_id = colliders[idx].get()->GetID();
            id.right_id = colliders[jdx].get()->GetID();

            auto colliderMapIter = m_mapColInfo.find(id.ID);

            //충돌 정보가 미 등록 상태일 경우.(충돌하지 않았다로 입력.) 
            if (colliderMapIter == m_mapColInfo.end()) {
                m_mapColInfo.insert(make_pair(id.ID, false));
                colliderMapIter = m_mapColInfo.find(id.ID);
            }

            //현재 Collider끼리 충돌했을 경우에. 
            if (colliders[idx]->Intersects(other)) {

                if (colliderMapIter->second == false) {
                    //이번 프레임에 막 충돌한 경우.

                    //TODO : 이벤트 후처리 시스템 어떻게? 
                    colliders[idx].get()->GetGameObject()->OnCollisionEnter(colliders[jdx].get()->GetGameObject());
                    colliders[jdx].get()->GetGameObject()->OnCollisionEnter(colliders[idx].get()->GetGameObject());
                    colliderMapIter->second = true;
                }
                else {
                    //이전 프레임에도 충돌하고 있던 경우. 
                    colliders[idx].get()->GetGameObject()->OnCollision(colliders[jdx].get()->GetGameObject());
                    colliders[jdx].get()->GetGameObject()->OnCollision(colliders[idx].get()->GetGameObject());
                }
            }
            else {
                //충돌하지 않았을 경우. 
                if (colliderMapIter->second == true) {
                    //이전에 충돌하고 있었으면. 
                    colliders[idx].get()->GetGameObject()->OnCollisionExit(colliders[jdx].get()->GetGameObject());
                    colliders[jdx].get()->GetGameObject()->OnCollisionExit(colliders[idx].get()->GetGameObject());
                    colliderMapIter->second = false;
                }
            }
        }
    }
}

void Scene::CheckCollisionWithQuadTree()
{
    auto quadTree = m_objectManager->GetQuadTree();
    if (!quadTree) return;

    quadTree->CheckCollisionsInTree(GetMainCamera()->GetCamera(), m_mapColInfo);
}


