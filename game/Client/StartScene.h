#pragma once

#include "Engine/Scene.h"

class GameObject;
class Cursor;
class SliderUI;
class CharacterSelectScene;

class StartScene :
    public Scene
{
public:
	virtual void Start();
	virtual void Update();
	virtual void FixedUpdate();
	virtual void LateUpdate();
	virtual void Render();


private:
	void CreateMainCamera();
	void CreateUICamera();
	void CreateLight();

	void CreateLobbyBackGround();
	void CreateSoundPanel();
	void CreateCursor();

	void LoadStartSceneImages();
	void LoadBtnImages();
	void LoadSliderImages();
	void LoadLobbyImages();

	void OnStartButtonClicked();
	void OnSoundButtonClicked();

	void EnableSoundPanel();
	void DisableSoundPanel();
	
	void OnBGMSliderMove();
	void OnSFXSliderMove();
	void OnButtonHover();

private:
	shared_ptr<Shader> m_defaultshader = nullptr;
	shared_ptr<Shader> m_imageShader = nullptr;
	shared_ptr<Cursor> m_cursor = nullptr;
	

	shared_ptr<GameObject> m_backPanel = nullptr;
	shared_ptr<GameObject> m_button = nullptr;
	shared_ptr<GameObject> m_testPanel = nullptr;


	shared_ptr<GameObject> m_soundButton = nullptr;

	shared_ptr<GameObject> m_soundBackPanel = nullptr;
	shared_ptr<GameObject> m_soundPanel = nullptr;
	shared_ptr<SliderUI> m_BGMSlider = nullptr;
	shared_ptr<SliderUI> m_SFXSlider = nullptr;
	shared_ptr<Button> m_soundDisableButton = nullptr;

	shared_ptr<GameObject> m_uiCamera = nullptr;


};


