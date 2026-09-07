#pragma once

class SceneObjectManager
{
public:
	SceneObjectManager();
	~SceneObjectManager();

public:
    void Start();
    void Update();
    void FixedUpdate();
    void LateUpdate();

public:
    //객체 추가 함수들
    virtual void Add(shared_ptr<GameObject> _object);                           //일반객체
    void AddUIObject(shared_ptr<GameObject> _object, bool isParent = false);    //UI객체
    virtual void Remove(shared_ptr<GameObject> _object);                        //객체 제거

    // 지연 삭제 함수들 
    void MarkForDestroy(shared_ptr<GameObject> obj);
    void MarkUIObjectForDestroy(shared_ptr<GameObject> obj);
    void MarkUIObjectForDestroyWithChildren(shared_ptr<GameObject> obj);
    void ProcessPendingDestroy();

    // UI 부모-자식 관계 등록
    void RegisterUIParent(shared_ptr<GameObject> parent);
    void RegisterUIChild(shared_ptr<GameObject> child);

    //피킹 관련 함수
    shared_ptr<class GameObject> PickObjectOrUI();
    shared_ptr<GameObject> PickObjectForAttack(shared_ptr<GameObject> _player);
    bool IsAttackableTarget(shared_ptr<GameObject> target);
    Ray CreateRayFromScreen(const Vec2& screenPos, shared_ptr<Camera> camera);


    //Getter & Setter
public:
    void SetSky(shared_ptr<Sky> _sky) { m_sky = _sky; }

    unordered_set<shared_ptr<GameObject>>& GetObjects() { return m_gameObjects; }
    unordered_set<shared_ptr<GameObject>>& GetUIObjects() { return m_uiObjects; }
    vector<shared_ptr<GameObject>>& GetUIChildren() { return m_uiChildren; }
    vector<shared_ptr<GameObject>>& GetUIParent() { return m_uiParents; }

    unordered_set<shared_ptr<GameObject>>& GetCameras() { return m_cameras; }
    shared_ptr<GameObject> GetMainCamera();
    shared_ptr<GameObject> GetUICamera();
    shared_ptr<GameObject> GetLight() { return m_Lights.empty() ? nullptr : *m_Lights.begin(); }
    weak_ptr<GameObject> GetPickedObj() { return m_curPickedObj; }

    //Util
public:
    string ws2s(const wstring& wstr);

    //QuadTree
public:
    void UpdateQuadTree();   
    QuadTree* GetQuadTree() { return m_quadTree.get(); }


private:
    void DestroyUIObjects();
    void DestroyNormalObjects();

    // 지연 삭제 처리 함수들
    void ProcessPendingNormalObjects();
    void ProcessPendingUIObjects();

    void CollectUIChildren(shared_ptr<GameObject> obj, vector<shared_ptr<GameObject>>& children);

    friend class Scene;

private:
    unordered_set<shared_ptr<GameObject>> m_gameObjects;    //UI를 제외한 모든 객체

    //UI 객체들 분리 저장
    unordered_set<shared_ptr<GameObject>> m_uiObjects;    // UI 객체들
    vector<shared_ptr<GameObject>> m_uiParents;           // UI 부모들 (PanelUI 등)
    vector<shared_ptr<GameObject>> m_uiChildren;          // UI 자식들 (ImageUI 등)

    //Cache Camera;
    unordered_set<shared_ptr<GameObject>> m_cameras;
    //Cache Light;
    unordered_set<shared_ptr<GameObject>> m_Lights;

    // 지연 삭제 컨테이너들 (새로 추가)
    vector<shared_ptr<GameObject>> m_pendingDestroyNormal;     // 일반 객체 삭제 대기열
    vector<shared_ptr<GameObject>> m_pendingDestroyUI;         // UI 객체 삭제 대기열
 
    shared_ptr<Sky> m_sky;
    weak_ptr<GameObject> m_curPickedObj;

    unique_ptr<QuadTree> m_quadTree;
    bool m_quadTreeDirty = true;
};

