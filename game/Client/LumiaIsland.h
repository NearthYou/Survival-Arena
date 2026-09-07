#pragma once
#include "Engine/Scene.h"
#include "IExecute.h"

#include "InventoryManager.h"
#include "UIManager.h"
#include "ItemSlot.h"

class GameObject;
class Player;
class Cursor;
class BiancaCamera;
class Wolf;
class Alpha;

class LumiaIsland :
    public Scene
{
	using Super = Scene;

public:
	LumiaIsland();
	virtual ~LumiaIsland();
public:

	virtual void Start() override;
	virtual void Update() override;
	virtual void FixedUpdate() override;
	virtual void LateUpdate() override;
	virtual void Render() override;

public:
	void SetAppearance(int index) { m_appearanceIndex = index; }
	void SetSelectedCharacter(int _idx) { m_selectedCharacterIdx = _idx; }

private:
    int m_appearanceIndex = 0;
	void CreateMainCamera();
	void CreateUICamera();
	void CreateDefaultLight();
	void SelectCharacter();
	void CreateAndSetUIManager();

	//땅, 인테리어, 환경 생성. 
	void CreateCemeteryBase();
	void CreateCemeteryInterior();
	void CreateCemeterySmallInterior();
	void CreateCemeteryEnvironment();
	void CreateCemeteryItemBox();
	// NavMesh 관련 추가
	void CreateNavMesh();
	void CreateCharacterNicky();
	void CreateCharacterBianca();

	shared_ptr<Wolf> CreateMonsterWolf(Vec3 _pos, Vec3 _rot = Vec3(0,0,0));
	shared_ptr<Alpha> CreateMonsterAlpha(Vec3 _pos, Vec3 _rot = Vec3(0,0,0));

	//=====================UI관련 함수=====================//
	void CreateCursor();

	void LoadItemBoxImages();
	void CreateItemBoxPanel();
	void CheckPickedItemBox();

	void OnItemBoxSlotClicked(int _slotIndex, SLOTTYPE _slotType);
	void UpdateItemBoxSlots(shared_ptr<GameObject> _itemBoxObject);
	//=====================UI관련 함수=====================//

	//=====================제작 완료 확인 함수=======================//
	bool IsCraftStateCompleted(); // 새로 추가
	//=====================제작 완료 확인 함수=======================//

	//=====================스킬 완료 확인 함수=======================//
	bool IsQSkillCompleted(); // 새로 추가
	bool IsWSkillCompleted();
	bool IsESkillCompleted();
	bool IsRSkillCompleted();
	bool IsCounterSkillCompleted();
	//=====================스킬 완료 확인 함수=======================//

	//=====================스킬 레벨업 함수=======================//
	void HandleSkillLevelUpInput();
	void LevelUpSkill(int skillIndex);
	//=====================스킬 레벨업 함수=======================//

private:
	//멀티 스레드 로딩용 함수.
	static DWORD WINAPI BackgroundLoadingThread(LPVOID _param);
	void ProcessMainThreadTasks();
	
private:
	//테스트용. 
	void CreateTestDecal();
	void ControlPlayerStatus();
	void CreateTestDummy();


private:
	shared_ptr<GameObject> m_CemeteryParent;

	//=====================카메라 관련 변수=====================//
	shared_ptr<BiancaCamera> m_cameraScript = nullptr;


	shared_ptr<Shader> m_defaultshader = nullptr;
	shared_ptr<Shader> m_testShader = nullptr;

	//=====================UI관련 변수=====================//
	shared_ptr<Cursor> m_cursor = nullptr;

	shared_ptr<GameObject> m_itemBox = nullptr;
	shared_ptr<GameObject> m_currentItemBox = nullptr;
	vector<shared_ptr<ItemSlot>> m_itemBoxSlots;


	//=====================UI관련 변수=====================//

	// 테스트용
	shared_ptr<GameObject> m_navMesh;
	
	shared_ptr<Player> m_player;


	int m_selectedCharacterIdx = 0; //0 : 비앙카 , 1 : 니키

	float m_lastFloatTime = 0.f;
	int m_lastTime = -1;
	shared_ptr<D2DText> m_test;

private:
	CRITICAL_SECTION m_loadingCS;
	HANDLE m_loadingThread;
	atomic<bool> m_loadingComplete{ false };

	//메인 스레드 작업 큐
	queue<function<void()>> m_mainThreadTasks;
	CRITICAL_SECTION m_mainThreadTasksCS;
	atomic<bool> m_objectsCreated{ false };

private:
	shared_ptr<UIManager> m_uiManager;
};



