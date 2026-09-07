#pragma once

class Sky;
class Camera;
class BaseCollider;
#include "QuadTree.h"
#include "SceneObjectManager.h"

union COLLIDER_ID {
    struct {
        uint32 left_id;
        uint32 right_id;
    };
    ULONG64 ID;
};

class Scene
{
public:
    Scene();
    ~Scene();

public:
    virtual void Start();
    virtual void Update();
    virtual void FixedUpdate();
    virtual void LateUpdate();
    virtual void Render();

    void RenderGameCamera(Camera* cam);
    void RenderUICamera(Camera* cam);

    virtual void Add(shared_ptr<GameObject> _object);
    // UI 객체 추가 (새로 추가)
    void AddUIObject(shared_ptr<GameObject> _object, bool isParent = false);
    virtual void Remove(shared_ptr<GameObject> _object);

    // UI 부모-자식 관계 등록
    void RegisterUIParent(shared_ptr<GameObject> parent);
    void RegisterUIChild(shared_ptr<GameObject> child);


    void SetSky(shared_ptr<Sky> _sky) { m_objectManager->SetSky(_sky); }

    unordered_set<shared_ptr<GameObject>>& GetObjects() { return m_objectManager->GetObjects(); }
    unordered_set<shared_ptr<GameObject>>& GetUIObjects() { return m_objectManager->GetUIObjects(); }
    vector<shared_ptr<GameObject>>& GetUIChildren() { return m_objectManager->GetUIChildren(); }
    vector<shared_ptr<GameObject>>& GetUIParent() { return m_objectManager->GetUIParent(); }

    shared_ptr<GameObject> GetMainCamera();
    shared_ptr<GameObject> GetUICamera();
    shared_ptr<GameObject> GetLight() { return m_objectManager->GetLight(); }

    unique_ptr<SceneObjectManager>& GetObjectManager() { return m_objectManager; }


    bool IsDestroying() { return m_isDestroying; }
    void SetDestroying(bool destroying) { m_isDestroying = destroying; }

    void CheckCollision();
    void CheckCollisionWithQuadTree();

public:
    QuadTree* GetQuadTree() { return m_objectManager->GetQuadTree(); }
    weak_ptr<GameObject> GetPickedObj() { return m_objectManager->GetPickedObj(); }


private:
    //충돌 관련 HashTable
    //충돌체 간의 이전 프레임 충돌. 
    unordered_map<ULONG64, bool> m_mapColInfo;

    //그룹간의 충돌 체크? 일단 보류. 

protected:
    shared_ptr<GameObject> m_pickedObject;
    shared_ptr<GameObject> m_player;

private:
    unique_ptr<SceneObjectManager> m_objectManager;
    bool m_isDestroying = false;  // 소멸 중인지 확인하는 플래그
};