#pragma once

#include "Scene.h"
class SceneManager
{
	DECLARE_SINGLE(SceneManager);

public:
	void Update();

	shared_ptr<Scene> GetCurScene() { return m_curScene; }


	template<typename T>
	void ChangeScene(shared_ptr<T> _scene) {
		/*m_curScene = _scene;
		m_curScene->Start();*/

		m_nextScene = _scene;
		m_sceneChangeRequested = true;
	}

	void ProcessSceneChange();

private:
	shared_ptr<Scene> m_curScene = make_shared<Scene>();
	shared_ptr<Scene> m_nextScene = nullptr;
	bool m_sceneChangeRequested = false;
};

