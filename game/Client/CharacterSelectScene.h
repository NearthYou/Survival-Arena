#pragma once
#include <array>

#include "Engine/Scene.h"

class GameObject;
class Cursor;

class CharacterSelectScene :
    public Scene
{
public:
	virtual void Start();
	virtual void Update();
	virtual void FixedUpdate();
	virtual void LateUpdate();
	virtual void Render();


private:
    int m_selectedAppearance = 0;
    std::array<shared_ptr<UIPanel>, 2> m_roleCards;
    shared_ptr<Text> m_countdownLabel;
	void CreateMainCamera();
	void CreateUICamera();
	void CreateLight();
	void CreateCursor();

	void CreateBackGround(); 
	void CreateScrollableCharacterList();
	void CreateScrollableSkinList(); // 스킨 목록 + full image

	void CreateTimeProgressBar();
	void UpdateTimeProgressBar();

	void CreateSelectedButton();
	
	void UpdateSkinList(shared_ptr<Button> button, int charIndex);
	void UpdateFullImage(shared_ptr<Button> button, int skinIndex);

	void LoadCharacterSelectSceneImages();

	void LoadBackGround();
	void LoadCharacterListSlotImages();
	void LoadCharacterSkinListSlotImages();
	void LoadCharacterImages();
	void LoadCharacterFullAndHalfImages();

	void OnCharacterImageButtonClicked(int charIndex);

	void OnCharacterSelectButtonClicked(int charIndex);
	void OnCharacterSelectButtonHover();


	void StartLumiaIsland();

private:
	shared_ptr<Shader> m_defaultshader = nullptr;
	shared_ptr<Shader> m_imageShader = nullptr;

	shared_ptr<Cursor> m_cursor = nullptr;
	
	shared_ptr<GameObject> m_backPanel = nullptr;
	shared_ptr<GameObject> m_characterList = nullptr;
	shared_ptr<GameObject> m_selectedCharacterSkinScrollView = nullptr;

private:
	shared_ptr<GameObject> m_timeProgressBar = nullptr;
	shared_ptr<UIPanel> m_timeProgressPanel = nullptr;
	shared_ptr<ImageUI> m_timeProgressUI = nullptr;
	Vec2 m_progressBarSize = Vec2(1800.f, 25.f); // 바 크기


private:
	shared_ptr<GameObject> m_charSelectBtn = nullptr;
	shared_ptr<UIPanel> m_charSelectPanel = nullptr;

private:
	int m_selectCharIdx = 0;
	float m_selectDuration = 55.f;
	float m_selectElapsedTime = 0.f;

	float m_countTimer = 0.f;
};


