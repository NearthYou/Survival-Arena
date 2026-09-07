#include "pch.h"
#include "SceneManager.h"

void SceneManager::Update()
{
    // Scene 변경 요청이 있으면 처리
    if (m_sceneChangeRequested) {
        ProcessSceneChange();
        return; // 이번 프레임은 새로운 Scene만 업데이트
    }

    if (m_curScene == nullptr)
        return;

    m_curScene->Update();
    m_curScene->FixedUpdate();
    m_curScene->LateUpdate();
    m_curScene->Render();
}

void SceneManager::ProcessSceneChange()
{
    if (m_nextScene == nullptr) {
        m_sceneChangeRequested = false;
        return;
    }

    // 현재 Scene 정리
    if (m_curScene) {
        // Scene 소멸 플래그 설정
        m_curScene->SetDestroying(true);

        // 모든 GameObject들의 OnDestroy 호출
        auto& objectManager = m_curScene->GetObjectManager();
        for (auto& obj : objectManager->GetObjects()) {
            if (obj) obj->OnDestroy();
        }
        for (auto& obj : objectManager->GetUIObjects()) {
            if (obj) obj->OnDestroy();
        }
    }

    // 새로운 Scene으로 전환
    m_curScene = m_nextScene;
    m_nextScene = nullptr;
    m_sceneChangeRequested = false;

    // 새로운 Scene 시작
    if (m_curScene) {
        m_curScene->Start();
    }
}