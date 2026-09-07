#pragma once
#include "IExecute.h"

class GameObject;

class UITestDemo : public IExecute
{

public:
	void Init() override;
	void Update() override;
	void Render() override;
	void ShowImguiTransform();


	void CreatePanelWithImageUI();

private:
	shared_ptr<Shader> m_shader;
	shared_ptr<GameObject> panelObj;
	shared_ptr<GameObject> nicky;
	bool m_TransformImgui = true;
};

