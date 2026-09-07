#include "pch.h"
#include "SceneObjectManager.h"
#include "GameObject.h"
#include "Camera.h"
#include "Sky.h"
#include "Light.h"
#include "Button.h"
#include "BaseCollider.h"
#include "SphereCollider.h"
#include "OBBBoxCollider.h"
#include "AABBBoxCollider.h"
#include "UIPanel.h"
#include "ImageUI.h"

SceneObjectManager::SceneObjectManager()
{

}

SceneObjectManager::~SceneObjectManager()
{
   
}

void SceneObjectManager::Start()
{
    const auto& objects = m_gameObjects;
    for (auto& object : objects) {
        object->Start();
    }

    const auto& uiObjects = m_uiObjects;
    for (auto& object : uiObjects) {
        object->Start();
    }
}

void SceneObjectManager::Update()
{
    const auto& objects = m_gameObjects;
    for (auto& object : objects) {
        object->Update();
    }

    const auto& uiObjects = m_uiObjects;
    for (auto& object : uiObjects) {
        object->Update();
    }
}

void SceneObjectManager::FixedUpdate()
{
    const auto& objects = m_gameObjects;
    for (auto& object : objects) {
        object->FixedUpdate();
    }
    const auto& uiObjects = m_uiObjects;
    for (auto& object : uiObjects) {
        object->FixedUpdate();
    }
}

void SceneObjectManager::LateUpdate()
{
    const auto& objects = m_gameObjects;
    for (auto& object : objects) {
        object->LateUpdate();
    }
    const auto& uiObjects = m_uiObjects;
    for (auto& object : uiObjects) {
        object->LateUpdate();
    }
}

void SceneObjectManager::Add(shared_ptr<GameObject> _object)
{
    // 일반 객체만 저장
    m_gameObjects.insert(_object);

    if (_object->GetCamera() != nullptr) {
        m_cameras.insert(_object);
    }

    if (_object->GetLight() != nullptr) {
        m_Lights.insert(_object);
    }
}

void SceneObjectManager::AddUIObject(shared_ptr<GameObject> _object, bool isParent)
{
    // UI 객체 컨테이너에 저장
    m_uiObjects.insert(_object);

    // 부모/자식 분류
    if (isParent) {
        m_uiParents.push_back(_object);
    }
    else {
        m_uiChildren.push_back(_object);
    }
}

void SceneObjectManager::Remove(shared_ptr<GameObject> _object)
{
    if (!_object || CURSCENE->IsDestroying()) return;

    // UI 객체인지 확인
    bool isUIObject = (m_uiObjects.find(_object) != m_uiObjects.end());

    if (isUIObject) {
        MarkUIObjectForDestroy(_object);
    }
    else {
        MarkForDestroy(_object);
    }
}
// 지연 삭제 함수들 구현
void SceneObjectManager::MarkForDestroy(shared_ptr<GameObject> obj)
{
    if (!obj || CURSCENE->IsDestroying()) return;

    // 중복 체크
    auto it = std::find(m_pendingDestroyNormal.begin(), m_pendingDestroyNormal.end(), obj);
    if (it == m_pendingDestroyNormal.end())
    {
        m_pendingDestroyNormal.push_back(obj);
    }
}

void SceneObjectManager::MarkUIObjectForDestroy(shared_ptr<GameObject> obj)
{
    if (!obj || CURSCENE->IsDestroying()) return;

    // 중복 체크
    auto it = std::find(m_pendingDestroyUI.begin(), m_pendingDestroyUI.end(), obj);
    if (it == m_pendingDestroyUI.end())
    {
        m_pendingDestroyUI.push_back(obj);
    }
}

void SceneObjectManager::MarkUIObjectForDestroyWithChildren(shared_ptr<GameObject> obj)
{
    if (!obj || CURSCENE->IsDestroying()) return;

    // 1. 자식 객체들 먼저 수집
    vector<shared_ptr<GameObject>> allChildren;
    CollectUIChildren(obj, allChildren);

    // 2. 자식들부터 삭제 마크 (역순으로)
    for (auto it = allChildren.rbegin(); it != allChildren.rend(); ++it) {
        MarkUIObjectForDestroy(*it);
    }

    // 3. 부모 객체 삭제 마크
    MarkUIObjectForDestroy(obj);
}

void SceneObjectManager::CollectUIChildren(shared_ptr<GameObject> obj, vector<shared_ptr<GameObject>>& children)
{
    if (!obj) return;

    // UIPanel의 자식들 수집
    if (auto panel = obj->GetUIPanel()) {
        for (auto& weakChild : panel->GetChildElements()) {
            if (auto child = weakChild.lock()) {
                children.push_back(child);
                // 재귀적으로 자식의 자식들도 수집
                CollectUIChildren(child, children);
            }
        }
    }

    // ImageUI의 레이어들 수집
    if (auto imageUI = obj->GetImageUI()) {
        for (auto& pair : imageUI->GetLayers()) {
            if (pair.second.gameObject) {
                children.push_back(pair.second.gameObject);
                // 레이어 객체의 자식들도 수집
                CollectUIChildren(pair.second.gameObject, children);
            }
        }
    }
}

void SceneObjectManager::ProcessPendingDestroy()
{
    if (CURSCENE->IsDestroying()) return;

    // UI 객체 먼저 처리
    ProcessPendingUIObjects();

    // 일반 객체 처리
    ProcessPendingNormalObjects();
}

void SceneObjectManager::ProcessPendingNormalObjects()
{
    if (m_pendingDestroyNormal.empty()) return;

    
    // 임시 벡터에 복사
    vector<shared_ptr<GameObject>> objectsToDestroy = m_pendingDestroyNormal;
    m_pendingDestroyNormal.clear();

    for (auto& obj : objectsToDestroy)
    {
        if (obj && obj.use_count() > 1)
        {
            // OnDestroy 호출
            obj->OnDestroy();
        }

        // 각 컨테이너에서 제거
        m_gameObjects.erase(obj);
        m_cameras.erase(obj);
        m_Lights.erase(obj);
    }
    
  
}

void SceneObjectManager::ProcessPendingUIObjects()
{
    if (m_pendingDestroyUI.empty()) return;
  
    // 임시 벡터에 복사
    vector<shared_ptr<GameObject>> objectsToDestroy = m_pendingDestroyUI;
    m_pendingDestroyUI.clear();

    for (auto& obj : objectsToDestroy)
    {
        if (obj && obj.use_count() > 1)
        {
            // OnDestroy 호출
            obj->OnDestroy();
        }

        // UI 컨테이너들에서 제거
        m_uiObjects.erase(obj);

        // vector에서 제거
        m_uiChildren.erase(std::remove(m_uiChildren.begin(), m_uiChildren.end(), obj), m_uiChildren.end());
        m_uiParents.erase(std::remove(m_uiParents.begin(), m_uiParents.end(), obj), m_uiParents.end());
    }    
}


void SceneObjectManager::RegisterUIParent(shared_ptr<GameObject> parent)
{
    auto it = std::find(m_uiParents.begin(), m_uiParents.end(), parent);
    if (it == m_uiParents.end())
    {
        m_uiParents.push_back(parent);
    }
}

void SceneObjectManager::RegisterUIChild(shared_ptr<GameObject> child)
{
    auto it = std::find(m_uiChildren.begin(), m_uiChildren.end(), child);
    if (it == m_uiChildren.end())
    {
        m_uiChildren.push_back(child);
    }
}

void SceneObjectManager::DestroyUIObjects()
{
    
    // 모든 UI 객체를 삭제 대기열에 추가
    for (auto& child : m_uiChildren) {
        MarkUIObjectForDestroy(child);
    }
    for (auto& parent : m_uiParents) {
        MarkUIObjectForDestroy(parent);
    }
    for (auto& uiObj : m_uiObjects) {
        MarkUIObjectForDestroy(uiObj);
    }

    // 컨테이너들 먼저 정리
    m_uiChildren.clear();
    m_uiParents.clear();
    m_uiObjects.clear();

    // 실제 삭제 처리
    ProcessPendingUIObjects();

    
   
}

void SceneObjectManager::DestroyNormalObjects()
{
   
    // 모든 일반 객체를 삭제 대기열에 추가
    for (auto& gameObject : m_gameObjects) {
        MarkForDestroy(gameObject);
    }

    // 컨테이너들 먼저 정리
    m_cameras.clear();
    m_Lights.clear();
    m_gameObjects.clear();

    // 실제 삭제 처리
    ProcessPendingNormalObjects();
    
   
}

shared_ptr<GameObject> SceneObjectManager::PickObjectOrUI()
{
    if (INPUT->GetButtonDown(KEY_TYPE::LBUTTON) == false)
        return nullptr;

    POINT screenPt = INPUT->GetMousePos();

    // UI 검사 (기존과 동일)
    if (GetUICamera() != nullptr)
    {
        const auto& gameObjects = m_uiObjects;
        for (auto& object : gameObjects)
        {
            if (object->GetActive() == false) continue;
            if (object->GetButton() == nullptr && object->GetUIPanel() == nullptr) continue;
            if (object->GetButton() != nullptr && object->GetButton()->Picked(screenPt))
            {
                //object->GetButton()->InvokeOnClicked();
                return object;
                //return nullptr;
            }
            if (object->GetUIPanel() != nullptr && object->GetUIPanel()->Picked(screenPt)) {
                return object;
            }
        }
    }

    shared_ptr<Camera> camera = GetMainCamera()->GetCamera();

    // Ray 생성
    Ray ray = CreateRayFromScreen(Vec2(screenPt.x, screenPt.y), camera);

    auto queryStart = std::chrono::high_resolution_clock::now();
    vector<shared_ptr<GameObject>> candidates = m_quadTree->Query(ray, camera);
    auto queryEnd = std::chrono::high_resolution_clock::now();

    // 좌표 비교 디버깅 + 음수 좌표 필터링
#if _DEBUG
    cout << "=== 좌표 비교 디버깅 ===" << endl;
#endif


    // 유효한 후보만 저장할 벡터
    vector<shared_ptr<GameObject>> validCandidates;

    for (auto& obj : candidates)
    {
        Vec3 worldPos = obj->GetTransform()->GetPosition();
        RECT objBounds = m_quadTree->GetObjectScreenBounds(obj, camera);

        // 화면 좌표 중심점 계산
        int screenCenterX = (objBounds.left + objBounds.right) / 2;
        int screenCenterY = (objBounds.top + objBounds.bottom) / 2;


        // 음수 좌표 검사
        if (screenCenterX < 0 || screenCenterY < 0)
        {
            //cout << "  -> 음수 좌표로 인해 후보에서 제외됨" << endl;
            continue; // 이 객체는 후보에서 제외
        }

#if _DEBUG
        cout << "객체 " << ws2s(obj->GetName()) << ":" << endl;
        cout << "  월드 좌표: (" << worldPos.x << ", " << worldPos.y << ", " << worldPos.z << ")" << endl;
        cout << "  화면 좌표: (" << screenCenterX << ", " << screenCenterY << ")" << endl;


        cout << "  마우스와 거리: " << sqrt(pow(screenPt.x - screenCenterX, 2) +
            pow(screenPt.y - screenCenterY, 2)) << endl;
#endif
        // 유효한 후보에 추가
        validCandidates.push_back(obj);
    }

#if _DEBUG
    // 성능 정보 출력 (수정된 후보 개수 반영)
    const auto& stats = m_quadTree->GetStats();
    cout << "=== 피킹 성능 정보 ===" << endl;
    cout << "전체 객체 수: " << m_gameObjects.size() << endl;
    cout << "초기 후보 객체 수: " << candidates.size() << endl;
    cout << "유효 후보 객체 수: " << validCandidates.size() << endl;
    cout << "필터링 효과: " << (candidates.size() - validCandidates.size()) << "개 제외" << endl;
    cout << "효율성: " << (100.0f * validCandidates.size() / (m_gameObjects.size() / 2)) << "%" << endl;
    cout << "쿼리 시간: " << stats.lastQueryTime.count() << "μs" << endl;
    cout << "마우스 좌표 : " << screenPt.x << " , " << screenPt.y << endl;
#endif
    // 유효한 후보들만 대상으로 교차 검사
    float minDistance = FLT_MAX;
    shared_ptr<GameObject> picked;

    for (auto& gameObject : validCandidates) // candidates -> validCandidates로 변경
    {
        if (camera->IsCulled(gameObject->GetLayerIndex())) continue;
        if (gameObject->GetCollider() == nullptr) continue;

        float distance = 0.f;
        if (gameObject->GetCollider()->Intersects(ray, OUT distance) == false) continue;

        if (distance < minDistance)
        {
            minDistance = distance;
            picked = gameObject;
        }
    }

    if (picked)//pick된 오브젝트. 
    {
        if (!ImGui::IsWindowHovered()) {
            m_curPickedObj = picked;
#if _DEBUG
            string name = ws2s(picked->GetName());
            cout << name << " : picked (distance: " << minDistance << ")" << endl;
#endif
        }
    }
    else {//버튼 눌렀으나 아무것도 picked되지 않았을 때. 
        //그러면서 Imgui창에도 클릭하지 않았을 때. 
        if (!ImGui::IsWindowHovered()) {
            m_curPickedObj.reset();
        }
    }

#if _DEBUG
    // 디버그 정보
    if (INPUT->GetButtonDown(KEY_TYPE::RBUTTON))
    {
        cout << "\n=== 상세 디버그 정보 ===" << endl;
        cout.flush();
        m_quadTree->DebugDraw(camera);
        m_quadTree->PrintDuplicates();
    }
#endif
    return picked;
}

//
string SceneObjectManager::ws2s(const wstring& wstr)
{
    string str;
    str.assign(wstr.begin(), wstr.end());
    return str;
}

// Ray 생성 
Ray SceneObjectManager::CreateRayFromScreen(const Vec2& screenPos, shared_ptr<Camera> camera)
{
    Viewport viewport = GRAPHICS->GetViewport();
    Matrix worldMatrix = Matrix::Identity;
    Matrix viewMatrix = camera->GetViewMatrix();
    Matrix projMatrix = camera->GetProjectionMatrix();

    // Near plane과 Far plane의 월드 좌표 계산
    Vec3 nearPoint = viewport.UnProject(Vec3(screenPos.x, screenPos.y, 0.0f), worldMatrix, viewMatrix, projMatrix);
    Vec3 farPoint = viewport.UnProject(Vec3(screenPos.x, screenPos.y, 1.0f), worldMatrix, viewMatrix, projMatrix);

    // Ray 방향 계산
    Vec3 rayDirection = farPoint - nearPoint;
    rayDirection.Normalize();

    return Ray(nearPoint, rayDirection);
}

void SceneObjectManager::UpdateQuadTree()
{
    // Synchronize camera matrices before caching screen bounds.
    GetMainCamera()->GetCamera()->UpdateMatrix();

    //Assimp용 임시. 
    //return;

    if (!m_quadTree)
    {
        float width = GRAPHICS->GetViewport().GetWidth();
        float height = GRAPHICS->GetViewport().GetHeight();
        m_quadTree = make_unique<QuadTree>(width, height);
    }

    // 카메라 변화 감지
    static Vec3 lastCameraPos = Vec3::Zero;
    static Vec3 lastCameraRot = Vec3::Zero;
    static int lastObjectCount = 0;

    //객체 위치 변화 감지를 위한 해시맵
    static unordered_map<shared_ptr<GameObject>, Vec3> lastObjectPositions;
    
    //객체의 Active감지를 위한 해시맵.
    static unordered_map<shared_ptr<GameObject>, bool> lastObjectActives;

    Vec3 currentCameraPos = GetMainCamera()->GetTransform()->GetPosition();
    Vec3 currentCameraRot = GetMainCamera()->GetTransform()->GetLocalRotation();
    int currentObjectCount = (int)m_gameObjects.size();

    // 변화 감지
    float positionDelta = Vec3::Distance(lastCameraPos, currentCameraPos);
    float rotationDelta = Vec3::Distance(lastCameraRot, currentCameraRot);

    //어떤 오브젝트라도 위치가 변경되었으면, UpdateQuadTree.
    bool objectMoved = false;
    bool objectChangeActive = false;
    for (auto& object : m_gameObjects) {
        if (!object->GetCollider()) continue;
        bool curActive = object->GetActive();
        auto activeIter = lastObjectActives.find(object);

        if (activeIter != lastObjectActives.end()) {
            if (activeIter->second != object->GetActive()) {
                objectChangeActive = true;
                break;
            }
        }
        else {
            objectChangeActive = true;
            break;
        }


        Vec3 curPos = object->GetTransform()->GetPosition();
        auto it = lastObjectPositions.find(object);
        if (it != lastObjectPositions.end()) {
            float prevCurDistance = Vec3::Distance(it->second, curPos);
            if (prevCurDistance > 0.1f) {
                objectMoved = true;
                break;
            }
        }
        else {
            //새로운 객체 발견 시.
            objectMoved = true;
            break;
        }
    }


    if (objectChangeActive || objectMoved || positionDelta > 0.1f || rotationDelta > 0.01f || currentObjectCount != lastObjectCount)
    {
        m_quadTreeDirty = true;
        lastCameraPos = currentCameraPos;
        lastCameraRot = currentCameraRot;
        lastObjectCount = currentObjectCount;
        objectMoved = false;
        objectChangeActive = false;

        for (auto& object : m_gameObjects) {
            if (object->GetCollider()) {
                lastObjectPositions[object] = object->GetTransform()->GetPosition();
                lastObjectActives[object] = object->GetActive();
            }
        }
    }

    if (m_quadTreeDirty)
    {
        auto buildStart = std::chrono::high_resolution_clock::now();

        m_quadTree->Clear();
        shared_ptr<Camera> camera = GetMainCamera()->GetCamera();

        // 객체 삽입 
        int insertedCount = 0;
        for (auto& object : m_gameObjects)
        {
            // 가시성 검사
            if (m_quadTree->IsObjectVisible(object, camera))
            {
                m_quadTree->Insert(object);
                insertedCount++;
            }
        }

        m_quadTree->Build();
        m_quadTreeDirty = false;

        auto buildEnd = std::chrono::high_resolution_clock::now();
        auto buildTime = std::chrono::duration_cast<std::chrono::microseconds>(buildEnd - buildStart);

//#ifdef _DEBUG
//        cout << "쿼드트리 재구성: " << insertedCount << "/" << m_gameObjects.size()
//            << " 객체, " << buildTime.count() << "μs" << endl;
//#endif
    }

}

shared_ptr<GameObject> SceneObjectManager::GetMainCamera()
{
    for (auto& camera : m_cameras) {
        if (camera->GetCamera()->GetProjectionType() == ProjectionType::Perspective)
            return camera;
    }
    return nullptr;
}

shared_ptr<GameObject> SceneObjectManager::GetUICamera()
{
    for (auto& camera : m_cameras) {
        if (camera->GetCamera()->GetProjectionType() == ProjectionType::Orthographic)
            return camera;
    }
    return nullptr;
}

// SceneObjectManager.cpp에 구현
shared_ptr<GameObject> SceneObjectManager::PickObjectForAttack(shared_ptr<GameObject> _player)
{
    if (INPUT->GetButtonDown(KEY_TYPE::RBUTTON) == false)
        return nullptr;

    POINT screenPt = INPUT->GetMousePos();

    // 기존 피킹 로직 재사용 (UI 검사 제외)
    shared_ptr<Camera> camera = GetMainCamera()->GetCamera();
    Ray ray = CreateRayFromScreen(Vec2(screenPt.x, screenPt.y), camera);

    // 쿼드트리를 이용한 후보 객체 수집
    vector<shared_ptr<GameObject>> candidates = m_quadTree->Query(ray, camera);
    vector<shared_ptr<GameObject>> validCandidates;

    // 음수 좌표 필터링
    for (auto& obj : candidates)
    {
        RECT objBounds = m_quadTree->GetObjectScreenBounds(obj, camera);
        int screenCenterX = (objBounds.left + objBounds.right) / 2;
        int screenCenterY = (objBounds.top + objBounds.bottom) / 2;

        if (screenCenterX < 0 || screenCenterY < 0) continue;
        validCandidates.push_back(obj);
    }

    // Ray 교차 검사로 가장 가까운 공격 가능한 대상 찾기
    float minDistance = FLT_MAX;
    shared_ptr<GameObject> pickedTarget;

    for (auto& gameObject : validCandidates)
    {
        if (camera->IsCulled(gameObject->GetLayerIndex())) continue;
        if (gameObject->GetCollider() == nullptr) continue;

        // 자기 자신 제외
        if (gameObject == /* 현재 플레이어 객체 */_player) continue;

        // 공격 가능한 대상인지 체크 (예: 몬스터, 다른 플레이어 등)
        if (!IsAttackableTarget(gameObject)) continue;

        float distance = 0.f;
        if (gameObject->GetCollider()->Intersects(ray, OUT distance) == false) continue;

        if (distance < minDistance)
        {
            minDistance = distance;
            pickedTarget = gameObject;
        }
    }

    return pickedTarget;
}

// 공격 가능한 대상인지 판단하는 헬퍼 함수
bool SceneObjectManager::IsAttackableTarget(shared_ptr<GameObject> target)
{
    // 구현 예시: 특정 태그나 컴포넌트를 가진 객체만 공격 가능
    // return target->HasTag("Enemy") || target->HasTag("Player");

    // 또는 특정 컴포넌트 존재 여부로 판단
    return target->GetMonsterStateMachine() != nullptr ||
        target->GetPlayerStateMachine() != nullptr;
}