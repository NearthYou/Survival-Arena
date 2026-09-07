#include "pch.h"

#include "LumiaIsland.h"
#include "ArenaAppearance.h"

#include "Cursor.h"

#include "BillboardDemo.h"
#include "BiancaTest.h"
#include "BiancaCamera.h"
#include "FogOfWar.h"
#include "CameraScript.h"

#include "AnimationStateMachine.h"
#include "PlayerStateMachine.h"
#include "SkillDecalIndicator.h"

#include "Player.h"

#include "Bianca.h"
#include "Nicky.h"

#include "Wolf.h"
#include "Alpha.h"

#include "NavMesh.h"
#include "NavMeshAgent.h"

#include "D2DText.h"
#include "ISkill.h"

#include "EquipableItem.h"
#include <string>
#include "GameHUDPanelUI.h"

#include "ItemManager.h"
#include "ItemBox.h"
#include "InventoryManager.h"

#include "Recipe.h"
#include "RecipeManager.h"

#include "CraftGagePanelUI.h"

#include "UIResourceManager.h"

#include "NickyCraftState.h"
#include "BiancaCraftState.h"

#include "NickyQState.h";
#include "NickyWState.h"
#include "NickyEState.h"
#include "NickyRState.h"
#include "NickyCounterState.h"

#include "BiancaQState.h"
#include "BiancaWState.h"
#include "BiancaEState.h"
#include "BiancaRState.h"

LumiaIsland::LumiaIsland()
{
	
}

LumiaIsland::~LumiaIsland()
{
	if (m_loadingThread) {
		WaitForSingleObject(m_loadingThread, INFINITE);
		CloseHandle(m_loadingThread);
	}

	DeleteCriticalSection(&m_loadingCS);
	DeleteCriticalSection(&m_mainThreadTasksCS);
}

void LumiaIsland::Start()
{
	TIME->ResetDeltaTime();

	// UI 리소스 매니저 초기화 (가장 먼저)
	UIResourceManager::GetInstance()->LoadAllUIResources();


	InitializeCriticalSection(&m_loadingCS);
	InitializeCriticalSection(&m_mainThreadTasksCS);

	m_defaultshader = make_shared<Shader>(L"FOW.fx");
	//CURSCENE->SetSky(make_shared<Sky>(L"..\\Resources\\Textures\\Sky\\skyBox.png", L"Sky.fx"));
	//m_testShader = make_shared<Shader>(L"23. RenderDemo.fx");
	CURSCENE->SetSky(make_shared<Sky>(L"..\\Resources\\Textures\\Sky\\skybox.dds", L"Sky.fx"));
	
	CreateMainCamera();
	CreateUICamera();
	CreateDefaultLight();
	
	SelectCharacter();

	m_loadingThread = CreateThread(nullptr, 0, BackgroundLoadingThread, this, 0, nullptr);

	
	CreateCemeteryBase();
	CreateCemeteryInterior();
	CreateCemeterySmallInterior();
	CreateCemeteryEnvironment();

	m_cameraScript->SetTarget(m_player);
	CreateCemeteryItemBox();
	//CreateTestDummy();

	//// NavMesh 생성 추가
	CreateNavMesh();


	////Monster 추가.
	{
		auto wolf = CreateMonsterWolf(Vec3(15, 18, 15.758));
		wolf->GetItemBox()->GetBoxInventory()[0] = ItemManager::GetInstance()->GetItem(L"쇠구슬");
	}
	{
		auto wolf = CreateMonsterWolf(Vec3(20, 18, 15.22));
		wolf->GetItemBox()->GetBoxInventory()[0] = ItemManager::GetInstance()->GetItem(L"운동화");
	}
	{
		auto wolf = CreateMonsterWolf(Vec3(45.587, 18, 49.758));
		wolf->GetItemBox()->GetBoxInventory()[0] = ItemManager::GetInstance()->GetItem(L"꽃");
	}
	{
		auto wolf = CreateMonsterWolf(Vec3(48.587, 18, 48.268));
		wolf->GetItemBox()->GetBoxInventory()[0] = ItemManager::GetInstance()->GetItem(L"원석");
	}
	{
		auto wolf = CreateMonsterWolf(Vec3(51.587, 18, 50.228));
		wolf->GetItemBox()->GetBoxInventory()[0] = ItemManager::GetInstance()->GetItem(L"델타 레드");
	}
	{
		auto wolf = CreateMonsterWolf(Vec3(72.0, 18, 13.228), Vec3(0.f, 135.f, 0.f));
		wolf->GetItemBox()->GetBoxInventory()[0] = ItemManager::GetInstance()->GetItem(L"붕대");
		wolf->GetItemBox()->GetBoxInventory()[1] = ItemManager::GetInstance()->GetItem(L"깃털");
		wolf->GetItemBox()->GetBoxInventory()[2] = ItemManager::GetInstance()->GetItem(L"돌멩이");
	}
	{
		CreateMonsterAlpha(Vec3(116.722, 18, 108.22));
	}

	TIME->ResetDeltaTime();
	SOUND->StopAll();
	SOUND->PlayBGM(L"BSER_AreaBGM_CEMETERY.wav", 0.5f);
	CreateCursor();
	Super::Start();

	// 기존 OnTryCraftFirst는 유지하되, 새로운 OnTryCraft도 추가 등록
	m_player->GetPlayerStateMachine()->OnTryCraftFirst.Push([this](bool& success) {
		// 기존 로직 유지
		auto inventoryMgr = InventoryManager::GetInstance();
		auto recipes = inventoryMgr->GetAvailableRecipes();

		if (!recipes.empty())
		{
			auto& slots = inventoryMgr->GetInventorySlots();

			auto GetCraftTimeByGrade = [](ITEMGRADE grade) -> float {
				switch (grade) {
				case ITEMGRADE::COMMON:    return 1.0f;
				case ITEMGRADE::UNCOMMON:  return 1.5f;
				case ITEMGRADE::RARE:      return 2.0f;
				case ITEMGRADE::EPIC:      return 3.0f;
				case ITEMGRADE::LEGENDARY: return 3.0f;
				default:                   return 5.0f;
				}
				};

			auto resultItem = ItemManager::GetInstance()->GetItem(recipes[0]->GetResultItemID());
			if (!resultItem) return;

			ITEMGRADE itemGrade = resultItem->GetItemGrade();

			m_player->GetPlayerStateMachine()->GetState(PlayerStateType::Craft)->SetRecipeIndex(0);
			m_player->GetAnimationStateMachine()->GetState(AnimationStateType::Craft)->SetExpectedDuration(GetCraftTimeByGrade(itemGrade));

			m_uiManager->GetCraftGageUI()->SetVisible(true);
			m_uiManager->GetCraftGageUI()->SetItem(resultItem);
			success = true;
		}
		else
			success = false;
		});

	// 새로운 OnTryCraft Delegate 등록 (더 간단한 버전)
	m_player->GetPlayerStateMachine()->OnTryCraft.Push([this](bool& success) {
		auto inventoryMgr = InventoryManager::GetInstance();
		auto recipes = inventoryMgr->GetAvailableRecipes();

		success = !recipes.empty();  // 제작 가능한 레시피가 있으면 true

		if (success)
		{
			// 제작 준비 작업
			auto resultItem = ItemManager::GetInstance()->GetItem(recipes[0]->GetResultItemID());
			if (resultItem)
			{
				// 제작 시간 설정
				auto GetCraftTimeByGrade = [](ITEMGRADE grade) -> float {
					switch (grade) {
					case ITEMGRADE::COMMON:    return 1.0f; 
					case ITEMGRADE::UNCOMMON:  return 3.0f; 
					case ITEMGRADE::RARE:      return 5.0f; 
					case ITEMGRADE::EPIC:      return 7.0f; 
					case ITEMGRADE::LEGENDARY: return 9.0f; 
					default:                   return 11.0f;
					}
					};

				ITEMGRADE itemGrade = resultItem->GetItemGrade();
				float craftTime = GetCraftTimeByGrade(itemGrade);

				// PlayerState에 레시피 인덱스 설정
				m_player->GetPlayerStateMachine()->GetState(PlayerStateType::Craft)->SetRecipeIndex(0);

				// AnimationState에 예상 시간 설정
				m_player->GetAnimationStateMachine()->GetState(AnimationStateType::Craft)->SetExpectedDuration(craftTime);

				// UI 업데이트
				m_uiManager->GetCraftGageUI()->SetVisible(true);
				m_uiManager->GetCraftGageUI()->SetItem(resultItem);
			}
		}
		});

	// === 새로운 OnCraftCompleted Delegate 등록 ===
	m_player->GetPlayerStateMachine()->OnCraftCompleted.Push([this](bool& completed) {
		// NickyCraftState의 완료 상태를 확인하는 로직
		completed = IsCraftStateCompleted();
		});

	// 기존 Delegate들은 유지하고 새로운 OnQSkillCompleted 추가
	m_player->GetPlayerStateMachine()->OnQSkillCompleted.Push([this](bool& completed) {
		// Q스킬 완료 상태를 확인하는 로직
		completed = IsQSkillCompleted();
		});

	// 기존 Delegate들은 유지하고 새로운 OnWSkillCompleted 추가
	m_player->GetPlayerStateMachine()->OnWSkillCompleted.Push([this](bool& completed) {
		// Q스킬 완료 상태를 확인하는 로직
		completed = IsWSkillCompleted();
		});

	// 기존 Delegate들은 유지하고 새로운 OnESkillCompleted 추가
	m_player->GetPlayerStateMachine()->OnESkillCompleted.Push([this](bool& completed) {
		// Q스킬 완료 상태를 확인하는 로직
		completed = IsESkillCompleted();
		});

	// 기존 Delegate들은 유지하고 새로운 OnRSkillCompleted 추가
	m_player->GetPlayerStateMachine()->OnRSkillCompleted.Push([this](bool& completed) {
		// Q스킬 완료 상태를 확인하는 로직
		completed = IsRSkillCompleted();
		});

	// 기존 Delegate들은 유지하고 새로운 OnCounterSkillCompleted 추가
	m_player->GetPlayerStateMachine()->OnCounterSkillCompleted.Push([this](bool& completed) {
		// Q스킬 완료 상태를 확인하는 로직
		completed = IsCounterSkillCompleted();
		});
}

void LumiaIsland::Update()
{
	ProcessMainThreadTasks();

	Super::Update();


	if (m_objectsCreated) {
		m_uiManager->Update();


		CheckPickedItemBox();
		ControlPlayerStatus();
		HandleSkillLevelUpInput();
	}
}

void LumiaIsland::FixedUpdate()
{
	Super::FixedUpdate();
}

void LumiaIsland::LateUpdate()
{
	Super::LateUpdate();
    // Debug geometry only: leave collider activation and collision tests unchanged.
    static bool referenceCollisionDebugVisible = false;
    if (INPUT->GetButtonDown(static_cast<KEY_TYPE>(VK_F3)))
        referenceCollisionDebugVisible = !referenceCollisionDebugVisible;
    for (const auto& object : GetObjects())
        if (const auto collider = object->GetCollider())
            collider->SetVisible(referenceCollisionDebugVisible);

}

void LumiaIsland::Render()
{
	Super::Render();
}

void LumiaIsland::CreateMainCamera()
{
	// Camera
	auto camera = make_shared<GameObject>();
	//camera->GetTransform()->SetPosition(Vec3(0.f, 15.f, 15.f));
	//camera->GetTransform()->SetPosition(Vec3{ 10.f, 30.f, -5.f });
	camera->GetTransform()->SetRotation(Vec3(45.f, -45.f, 0.f));
	camera->AddComponent(make_shared<Camera>());
	//camera->AddComponent(make_shared<CameraScript>());
	// 
	m_cameraScript = make_shared<BiancaCamera>();
	camera->AddComponent(m_cameraScript);

	camera->GetCamera()->SetCullingMaskLayerOnOff(LAYER_UI, true);
	CURSCENE->Add(camera);
}

void LumiaIsland::CreateUICamera()
{
	// UICamera
	auto camera = make_shared<GameObject>();
	camera->GetTransform()->SetPosition(Vec3{ 0.f, 0.f, -5.f });
	camera->AddComponent(make_shared<Camera>());
	camera->GetCamera()->SetProjectionType(ProjectionType::Orthographic);
	camera->GetCamera()->SetNear(0.1f);
	camera->GetCamera()->SetFar(100.0f);
	camera->GetCamera()->SetCullingMaskAll();
	camera->GetCamera()->SetCullingMaskLayerOnOff(LAYER_UI, false);
	//CURSCENE->Add(camera);
	Add(camera);
}

void LumiaIsland::CreateDefaultLight()
{
	//Default Light	
	// Light
	auto light = make_shared<GameObject>();
	light->AddComponent(make_shared<Light>());

	LightDesc lightDesc;
	lightDesc.ambient = Vec4(0.4f);
	lightDesc.diffuse = Vec4(1.f);
	lightDesc.specular = Vec4(0.1f);
	Vec3 lightDirection = Vec3(1.f, -1.f, 1.f); // Y를 음수로 (아래쪽을 향하도록)
	lightDirection.Normalize();
	lightDesc.direction = lightDirection;
	//light->GetLight()->SetLightDesc(lightDesc);
	light->GetTransform()->SetPosition(Vec3(0.f, 150.f, 0.f));
	Vec3 normalizedDir = Vec3(1.f, -1.f, 1.f);
	normalizedDir.Normalize();
	lightDesc.direction = normalizedDir;
	//light->GetTransform()->SetRotation(lightDesc.direction);
	//light->GetTransform()->SetPosition(Vec3(0.f, 150.f, 0.f));

	static_pointer_cast<Light>(light->GetFixedComponent(ComponentType::Light))->SetLightDesc(lightDesc);
	Add(light);
}

void LumiaIsland::SelectCharacter()
{
	//테스트용 임시 강제 설정
	//m_selectedCharacterIdx = 1;


	if (m_selectedCharacterIdx == 0) {
		CreateCharacterBianca();
	}
	else if (m_selectedCharacterIdx == 1) {
		CreateCharacterNicky();
	}

	//UI Manager는 항상 Player가 있어야됨
	if (m_player) m_player->SetAppearanceTint(ArenaAppearance::Tint(m_appearanceIndex));
	CreateAndSetUIManager();
}

void LumiaIsland::CreateAndSetUIManager()
{
	m_uiManager = make_shared<UIManager>(m_player, m_selectedCharacterIdx); //플레이가 존재할때 선언
	m_player->SetUIManager(m_uiManager);
}

void LumiaIsland::CreateCemeteryBase()
{
	m_CemeteryParent = make_shared<GameObject>();
	m_CemeteryParent->SetName(L"Cemetery_Parent");
	CURSCENE->Add(m_CemeteryParent);

	shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_Base", L"Cemetery/Cemetery_STR_Base");
	//m2->ReadModel(L"forest/forest");
	m2->ReadMaterial(L"Cemetery/Cemetery_STR_Base");
	auto obj = make_shared<GameObject>();
	obj->SetName(L"Cemetery_STR_Base");
	obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
	obj->GetTransform()->SetLocalPosition(Vec3(0, 0, 0));
	obj->GetTransform()->SetLocalRotation(Vec3(0, 0.f, 0));
	//obj->AddComponent(make_shared<SphereCollider>());
	obj->GetTransform()->SetLocalScale(Vec3(0.02f));
	obj->SetType(OBJECTTYPE::MAP);

	obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
	{
		obj->GetModelRenderer()->SetModel(m2);
		obj->GetModelRenderer()->SetPass(1);
	}

	CURSCENE->Add(obj);

	//ChurchBase
	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"ChurchBase", L"map2/ChurchBase");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"map2/ChurchBase");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"ChurchBase");
		obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(-76.3f, 17.5f, -52.f));
		obj->GetTransform()->SetLocalRotation(Vec3(0, 0.f, 0));
		obj->AddComponent(make_shared<SphereCollider>());
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::MAP);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}

		CURSCENE->Add(obj);
	}

	//ChurchBase
	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"FactoryBase", L"map2/FactoryBase");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"map2/FactoryBase");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"FactoryBase");
		obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(-76.3f, 17.5f, -52.f));
		obj->GetTransform()->SetLocalRotation(Vec3(0, 0.f, 0));
		obj->AddComponent(make_shared<SphereCollider>());
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::MAP);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}

		CURSCENE->Add(obj);
	}

	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"HospitalBase", L"map2/HospitalBase");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"map2/HospitalBase");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"HospitalBase");
		obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(-76.3f, 17.f, -54.5f));
		obj->GetTransform()->SetLocalRotation(Vec3(0, 0.f, 0));
		obj->AddComponent(make_shared<SphereCollider>());
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::MAP);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}

		CURSCENE->Add(obj);
	}


}

void LumiaIsland::CreateCemeteryInterior()
{
	//OuterWall
	{
		//Cemetery_STR_OuterWall_02
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_OuterWall_02", L"Cemetery/Cemetery_STR_OuterWall_02");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_OuterWall_02");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_OuterWall_02");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(125.919, 17.859, 105.524));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_OuterWall_02_Fence
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_OuterWall_02_Fence", L"Cemetery/Cemetery_STR_OuterWall_02_Fence");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_OuterWall_02_Fence");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_OuterWall_02_Fence");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(125.919, 17.859, 105.524));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_GraveBase_02
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_GraveBase_02", L"Cemetery/Cemetery_STR_GraveBase_02");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_GraveBase_02");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_GraveBase_02");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(125.919, 17.859, 105.524));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_OuterWall_02_Fence
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_OuterWall_02_Grass", L"Cemetery/Cemetery_STR_OuterWall_02_Grass");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_OuterWall_02_Grass");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_OuterWall_02_Grass");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(125.919, 18.1, 105.524));
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}


		//Cemetery_STR_InnerWall_02
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_InnerWall_02", L"Cemetery/Cemetery_STR_InnerWall_02");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_InnerWall_02");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_InnerWall_02");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(125.919, 17.859, 105.524));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}
	}


	{
		//Cemetery_STR_OuterWall_03
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_OuterWall_03", L"Cemetery/Cemetery_STR_OuterWall_03");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_OuterWall_03");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_OuterWall_03");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(158.7, 17.859, 63.083));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_OuterWall_03_Fence
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_OuterWall_03_Fence", L"Cemetery/Cemetery_STR_OuterWall_03_Fence");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_OuterWall_03_Fence");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_OuterWall_03_Fence");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(158.7, 17.859, 63.083));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_House_01_Wall
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_House_01_Wall", L"Cemetery/Cemetery_STR_House_01_Wall");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_House_01_Wall");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_House_01_Wall");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(142.178, 17.859, 29.834));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_House_01
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_House_01", L"Cemetery/Cemetery_STR_House_01");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_House_01");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_House_01");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(142.178, 17.859, 29.834));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_House_01_Interior
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_House_01_Interior", L"Cemetery/Cemetery_STR_House_01_Interior");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_House_01_Interior");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_House_01_Interior");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(142.178, 17.859, 29.834));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_House_01_Wall_Grass
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_House_01_Wall_Grass", L"Cemetery/Cemetery_STR_House_01_Wall_Grass");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_House_01_Wall_Grass");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_House_01_Wall_Grass");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(142.178, 17.1, 29.834));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

	}
	// TextDeco_Set
	{
		//Bg_Cemetery_STR_TextDeco_Set
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Bg_Cemetery_STR_TextDeco_Set", L"Cemetery/Bg_Cemetery_STR_TextDeco_Set");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Bg_Cemetery_STR_TextDeco_Set");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Bg_Cemetery_STR_TextDeco_Set");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(114, 24.7, 26.665));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Bg_Cemetery_STR_TextDeco_Set_01
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Bg_Cemetery_STR_TextDeco_Set", L"Cemetery/Bg_Cemetery_STR_TextDeco_Set");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Bg_Cemetery_STR_TextDeco_Set");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Bg_Cemetery_STR_TextDeco_Set_01");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(120.843, 24.965, 113));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}
	}


	//Cemetery_STR_OuterWall_04
	{
		//Cemetery_STR_OuterWall_04
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_OuterWall_04", L"Cemetery/Cemetery_STR_OuterWall_04");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_OuterWall_04");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_OuterWall_04");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(64.588, 17.859, 26.163));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_OuterWall_04_Fence
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_OuterWall_04_Fence", L"Cemetery/Cemetery_STR_OuterWall_04_Fence");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_OuterWall_04_Fence");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_OuterWall_04_Fence");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(64.588, 17.859, 26.163));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}
	}

	//Cemetery_STR_GrabeBase_03_Wall_01
	{

		//Cemetery_STR_GrabeBase_03_Wall_01
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_GraveBase_03_Wall_01", L"Cemetery/Cemetery_STR_GraveBase_03_Wall_01");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_GraveBase_03_Wall_01");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_GraveBase_03_Wall_01");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(92, 17.859, 55.934));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_GraveBase_03_Wall_01_Fence
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_GraveBase_03_Wall_01_Fence", L"Cemetery/Cemetery_STR_GraveBase_03_Wall_01_Fence");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_GraveBase_03_Wall_01_Fence");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_GraveBase_03_Wall_01_Fence");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(92, 17.859, 55.934));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_GraveBase_03_Wall_02
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_GraveBase_03_Wall_02", L"Cemetery/Cemetery_STR_GraveBase_03_Wall_02");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_GraveBase_03_Wall_02");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_GraveBase_03_Wall_02");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(92, 17.429, 55.934));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_GraveBase_03_Wall_02_01
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_GraveBase_03_Wall_02", L"Cemetery/Cemetery_STR_GraveBase_03_Wall_02");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_GraveBase_03_Wall_02");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_GraveBase_03_Wall_02");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(77.3, 17.429, 55.934));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}
	}


	//ETC
	{
		//Cemetery_STR_InnerWall_04
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_InnerWall_04", L"Cemetery/Cemetery_STR_InnerWall_04");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_InnerWall_04");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_InnerWall_04");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(78.7, 17.865, 5.23));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_InnerWall_05
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_InnerWall_05", L"Cemetery/Cemetery_STR_InnerWall_05");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_InnerWall_05");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_InnerWall_05");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(78.7, 17.865, 5.23));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}
	}


	//병원, 물가쪽 벽. 
	{
		//Cemetery_STR_OuterWall_01
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_OuterWall_01", L"map2/Cemetery_STR_OuterWall_01");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"map2/Cemetery_STR_OuterWall_01");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_OuterWall_01");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(86.014, 17.859, 113.655));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_OuterWall_01_Fence
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_OuterWall_01_Fence", L"map2/Cemetery_STR_OuterWall_01_Fence");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"map2/Cemetery_STR_OuterWall_01_Fence");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_OuterWall_01_Fence");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(86.014, 16.919, 113.655));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_OuterWall_01_Grass
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_OuterWall_01_Grass", L"map2/Cemetery_STR_OuterWall_01_Grass");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"map2/Cemetery_STR_OuterWall_01_Grass");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_OuterWall_01_Grass");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(85.961, 17.859, 113.655));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.025f, 0.02f, 0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}


		//Cemetery_STR_InnerWall_01
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_InnerWall_01", L"map2/Cemetery_STR_InnerWall_01");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"map2/Cemetery_STR_InnerWall_01");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_InnerWall_01");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(86.014, 17.859, 113.655));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//Cemetery_STR_InnerWall_01
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_GraveBase_01", L"map2/Cemetery_STR_GraveBase_01");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"map2/Cemetery_STR_GraveBase_01");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_GraveBase_01");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(86.014, 17.859, 113.655));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}
	}
}

void LumiaIsland::CreateCemeterySmallInterior()
{

	//Cemetery 숲 쪽, 4개 묘비 모여있는 곳. 
	{
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tombstone_08", L"Cemetery/Cemetery_OBJ_Tombstone_08");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tombstone_08");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_OBJ_Tombstone_08");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(29.2, 18, 27));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, 0.223f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tombstone_07", L"Cemetery/Cemetery_OBJ_Tombstone_07");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tombstone_07");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_OBJ_Tombstone_07");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(26.95, 18, 24));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, 0.223f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Common_OBJ_BrickDecoTop_03_Broken_01", L"Cemetery/Common_OBJ_BrickDecoTop_03_Broken_01");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Common_OBJ_BrickDecoTop_03_Broken_01");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Common_OBJ_BrickDecoTop_03_Broken_01");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(27.92, 20.2, 22.05));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, 40.f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.035f, 0.035f, 0.035f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tombstone_08", L"Cemetery/Cemetery_OBJ_Tombstone_08");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tombstone_08");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_OBJ_Tombstone_08");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(28.32, 18.2, 25.07));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, 40.f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.023f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		//집. 
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_House_02", L"map2/Cemetery_STR_House_02");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"map2/Cemetery_STR_House_02");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_House_02");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(38.846, 18.2, 22.12));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, 0.f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}
	}

	//물가쪽. 
	{
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tombstone_08", L"Cemetery/Cemetery_OBJ_Tombstone_08");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tombstone_08");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_OBJ_Tombstone_08");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(24.28, 17.648, 55.89));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, 120.f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.03f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Common_OBJ_BrickDecoTop_03_Broken_01", L"Cemetery/Common_OBJ_BrickDecoTop_03_Broken_01");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Common_OBJ_BrickDecoTop_03_Broken_01");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Common_OBJ_BrickDecoTop_03_Broken_01");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(38.762, 19.648, 21.991));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, 14.785f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.023f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Common_OBJ_BrickDecoTop_03_Broken_01", L"Cemetery/Common_OBJ_BrickDecoTop_03_Broken_01");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Common_OBJ_BrickDecoTop_03_Broken_01");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Common_OBJ_BrickDecoTop_03_Broken_01");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(40.662, 19.648, 21.991));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, 14.785f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.023f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}
	}


	//묘지쪽.

	{
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tomb_03", L"Cemetery/Cemetery_OBJ_Tomb_03");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tomb_03");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_OBJ_Tomb_03");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(79.817, 18, 94.383));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tomb_04", L"Cemetery/Cemetery_OBJ_Tomb_04");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tomb_04");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_OBJ_Tomb_04");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(68.3, 18, 94.07));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tomb_04", L"Cemetery/Cemetery_OBJ_Tomb_04");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tomb_04");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_OBJ_Tomb_04");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(68.3, 18, 94.07));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tombstone_08", L"Cemetery/Cemetery_OBJ_Tombstone_08");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tombstone_08");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_OBJ_Tombstone_08");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(72.507, 17.8, 94.03));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, 90.f, -20.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.03f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}
	}



}

void LumiaIsland::CreateCemeteryEnvironment()
{
	//가운데 기준 11시쪽 무덤 - 1
	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tombstone_02", L"Cemetery/Cemetery_OBJ_Tombstone_02");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tombstone_02");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"Cemetery_OBJ_Tombstone_01");
		obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(96.002, 20, 79.367));
		obj->GetTransform()->SetLocalRotation(Vec3(0.f, 5486.223f, 0.f));
		//obj->AddComponent(make_shared<AABBBoxCollider>());
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::MAP);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}
		CURSCENE->Add(obj);
	}



	//가운데 기준 5시쪽 무덤 - 1
	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tombstone_02", L"Cemetery/Cemetery_OBJ_Tombstone_02");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tombstone_02");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"Cemetery_OBJ_Tombstone_01");
		obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(124.576, 20.117, 44.456));
		obj->GetTransform()->SetLocalRotation(Vec3(0.f, 2557.334, 0.f));
		//obj->AddComponent(make_shared<AABBBoxCollider>());
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::MAP);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}
		CURSCENE->Add(obj);
	}


	//가운데 기준 11시쪽 무덤 - 2
	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tombstone_03", L"Cemetery/Cemetery_OBJ_Tombstone_03");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tombstone_03");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"Cemetery_OBJ_Tombstone_03");
		obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(83.649, 20, 79.549));
		obj->GetTransform()->SetLocalRotation(Vec3(0.f, 4395.447, 0.f));
		//obj->AddComponent(make_shared<AABBBoxCollider>());
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::MAP);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}
		CURSCENE->Add(obj);
	}



	//가운데 기준 5시시쪽 무덤 - 2
	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tombstone_04", L"Cemetery/Cemetery_OBJ_Tombstone_04");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tombstone_04");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"Cemetery_OBJ_Tombstone_04");
		obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(120.567, 20, 44.595));
		obj->GetTransform()->SetLocalRotation(Vec3(0.f, -1263.546, 0.f));
		//obj->AddComponent(make_shared<AABBBoxCollider>());
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::MAP);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}
		CURSCENE->Add(obj);
	}

	//가운데 기준 5시시쪽 무덤 - 3
	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tombstone_04", L"Cemetery/Cemetery_OBJ_Tombstone_04");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tombstone_04");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"Cemetery_OBJ_Tombstone_04");
		obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(110.770, 20, 44.711));
		obj->GetTransform()->SetLocalRotation(Vec3(0.f, 0.f, 0.f));
		//obj->AddComponent(make_shared<AABBBoxCollider>());
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::MAP);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}
		CURSCENE->Add(obj);
	}




	//가운데 기준 1시시쪽 무덤 - 1
	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tomb_02", L"Cemetery/Cemetery_OBJ_Tomb_02");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tomb_02");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"Cemetery_OBJ_Tomb_02");
		obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(124.497, 20, 69.716));
		obj->GetTransform()->SetLocalRotation(Vec3(0.f, -47.64, 0.f));
		//obj->AddComponent(make_shared<AABBBoxCollider>());
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::MAP);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}
		CURSCENE->Add(obj);
	}

	//가운데 기준 7시시쪽 무덤 - 1
	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tomb_03", L"Cemetery/Cemetery_OBJ_Tomb_03");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tomb_03");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"Cemetery_OBJ_Tomb_03");
		obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(83.009, 21.389, 45.124));
		obj->GetTransform()->SetLocalRotation(Vec3(0.f, 169, 0.f));
		//obj->AddComponent(make_shared<AABBBoxCollider>());
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::MAP);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}
		CURSCENE->Add(obj);
	}


	//가운데 기준 11시시쪽 무덤 - 3
	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_OBJ_Tomb_03", L"Cemetery/Cemetery_OBJ_Tomb_03");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"Cemetery/Cemetery_OBJ_Tomb_03");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"Cemetery_OBJ_Tomb_03");
		obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(83.232, 20, 71.818));
		obj->GetTransform()->SetLocalRotation(Vec3(0.f, 169, 0.f));
		//obj->AddComponent(make_shared<AABBBoxCollider>());
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::MAP);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}
		CURSCENE->Add(obj);
	}

	//나무 배치. 
	{
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Tree_Ancient_01", L"map2/Tree_Ancient_01");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"map2/Tree_Ancient_01");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Tree_Ancient_01");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(15.69, 19.728, 37.754));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, 0.f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Tree_Ancient_01", L"map2/Tree_Ancient_01");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"map2/Tree_Ancient_01");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Tree_Ancient_01");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(-12.094, 15.202, 29.772));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, -180.f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);

		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Tree_Ancient_03", L"map2/Tree_Ancient_03");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"map2/Tree_Ancient_03");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Tree_Ancient_03");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(6.26, 18.798, 54.7));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, 0.f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);

		}



		//병원쪽. 
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Tree_04_Re", L"map2/Tree_04_Re");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"map2/Tree_04_Re");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Tree_04_Re");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(86.34, 18.628, 111.1));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, -180.f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);

		}


		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Tree_05_Re", L"map2/Tree_05_Re");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"map2/Tree_05_Re");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Tree_05_Re");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(86.34, 18.628, 101.1));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, -180.f, 0.f));
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);

		}



		//병원보다는 호수쪽. 
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Tree_08_Re", L"map2/Tree_08_Re");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"map2/Tree_08_Re");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Tree_08_Re");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(56.458, 19.19, 74.775));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, -180.f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Tree_08_Re", L"map2/Tree_08_Re");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"map2/Tree_08_Re");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Tree_08_Re");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(32.5, 19.19, 54.9));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, -90.f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Tree_08_Re", L"map2/Tree_08_Re");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"map2/Tree_08_Re");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Tree_08_Re");
			obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
			obj->GetTransform()->SetLocalPosition(Vec3(46.284, 19.19, 58.488));
			obj->GetTransform()->SetLocalRotation(Vec3(0.f, 0.f, 0.f));
			//obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}
			CURSCENE->Add(obj);
		}
	}

}

void LumiaIsland::CreateCemeteryItemBox()
{
	//가운데 기준 11시시쪽 무덤 - 3
	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"ItemBox", L"Cemetery/ItemBox");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"Cemetery/ItemBox");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"ItemBox_01");
		//obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(96.9, 19.5, 59));
		obj->GetTransform()->SetLocalRotation(Vec3(0.f, 0.f, 0.f));
		obj->AddComponent(make_shared<AABBBoxCollider>());
		obj->GetCollider()->SetOffsetScale(Vec3(150.f));
		obj->AddComponent(make_shared<ItemBox>());
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::ITEMBOX);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}

		obj->GetComponent<ItemBox>()->GetBoxInventory()[0] = ItemManager::GetInstance()->GetItem(L"운명의 수레바퀴");
		CURSCENE->Add(obj);
	}

	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"ItemBox", L"Cemetery/ItemBox");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"Cemetery/ItemBox");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"ItemBox_02");
		//obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(96.9, 20, 65.94));
		obj->GetTransform()->SetLocalRotation(Vec3(0.f, 180.f, 0.f));
		obj->AddComponent(make_shared<AABBBoxCollider>());
		obj->AddComponent(make_shared<ItemBox>());
		obj->GetCollider()->SetOffsetScale(Vec3(150.f));
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::ITEMBOX);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}
		obj->GetComponent<ItemBox>()->GetBoxInventory()[0] = ItemManager::GetInstance()->GetItem(L"비질란테");
		CURSCENE->Add(obj);
	}

	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"ItemBox", L"Cemetery/ItemBox");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"Cemetery/ItemBox");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"ItemBox_03");
		//obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(111.6, 19.5, 59));
		obj->GetTransform()->SetLocalRotation(Vec3(0.f, 0.f, 0.f));
		obj->AddComponent(make_shared<AABBBoxCollider>());
		obj->AddComponent(make_shared<ItemBox>());
		obj->GetCollider()->SetOffsetScale(Vec3(150.f));
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::ITEMBOX);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}
		obj->GetComponent<ItemBox>()->GetBoxInventory()[0] = ItemManager::GetInstance()->GetItem(L"어사의");
		CURSCENE->Add(obj);
	}

	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"ItemBox", L"Cemetery/ItemBox");
		//m2->ReadModel(L"forest/forest");
		m2->ReadMaterial(L"Cemetery/ItemBox");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"ItemBox_04");
		//obj->GetTransform()->SetParent(m_CemeteryParent->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(111.6, 20, 65.94));
		obj->GetTransform()->SetLocalRotation(Vec3(0.f, 180.f, 0.f));
		obj->AddComponent(make_shared<AABBBoxCollider>());
		obj->AddComponent(make_shared<ItemBox>());
		obj->GetCollider()->SetOffsetScale(Vec3(150.f));
		obj->GetTransform()->SetLocalScale(Vec3(0.02f));
		obj->SetType(OBJECTTYPE::ITEMBOX);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}
		CURSCENE->Add(obj);
	}

}

void LumiaIsland::CreateNavMesh()
{
	// Animation
	shared_ptr<Model> m1 = make_shared<Model>();

	m1->ReadModel(L"NavMesh/NavMesh");
	m1->ReadMaterial(L"NavMesh/NavMesh");


	for (int32 i = 0; i < 1; i++)
	{

		m_navMesh = make_shared<GameObject>();
		m_navMesh->SetName(to_wstring(i));

		m_navMesh->GetTransform()->SetPosition(Vec3(-75.7, 18, -54));
		//m_navMesh->GetTransform()->SetPosition(Vec3(0, 18, 0));
		m_navMesh->GetTransform()->SetScale(Vec3(2.f));
		m_navMesh->GetTransform()->SetLocalRotation(Vec3(270.f, 270.f, 90.f));

		m_navMesh->AddComponent(make_shared<SphereCollider>());
		m_navMesh->AddComponent(make_shared<Rigidbody>());
		m_navMesh->GetCollider()->SetOffset(Vec3(0.f, 1.f, 0.f));
		m_navMesh->GetRigidbody()->SetStatic(true);
		m_navMesh->SetType(OBJECTTYPE::MAP);

		m_navMesh->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			m_navMesh->GetModelRenderer()->SetModel(m1);
			m_navMesh->GetModelRenderer()->SetPass(1);
		}

		m_navMesh->AddComponent(make_shared<NavMesh>());
		
	
		CURSCENE->Add(m_navMesh);
	}
}

void LumiaIsland::CreateCharacterNicky()
{
	shared_ptr<Nicky> nicky = make_shared<Nicky>(m_defaultshader);
	nicky->SetName(L"Nicky");
	nicky->GetTransform()->SetPosition(Vec3(15, 18, 5));
	nicky->GetTransform()->SetScale(Vec3(2.f));
	nicky->SetType(OBJECTTYPE::PLAYER);
	
	m_selectedCharacterIdx = 1;

	m_player = nicky;

	CURSCENE->Add(nicky);
}


void LumiaIsland::CreateCharacterBianca()
{
	shared_ptr<Bianca> bianca = make_shared<Bianca>(m_defaultshader);
	bianca->GetTransform()->SetPosition(Vec3(15, 18, 5));
	bianca->GetTransform()->SetScale(Vec3(2.f));
	bianca->SetType(OBJECTTYPE::PLAYER);
	m_selectedCharacterIdx = 0;

	m_player = bianca;


	CURSCENE->Add(bianca);
}

shared_ptr<Wolf> LumiaIsland::CreateMonsterWolf(Vec3 _pos, Vec3 _rot)
{
	shared_ptr<Wolf> wolf = make_shared<Wolf>(m_defaultshader);

	wolf->GetTransform()->SetPosition(_pos);
	wolf->GetTransform()->SetRotation(_rot);
	wolf->GetTransform()->SetScale(Vec3(2.f));
	CURSCENE->Add(wolf);

	return wolf;
}

shared_ptr<Alpha> LumiaIsland::CreateMonsterAlpha(Vec3 _pos, Vec3 _rot)
{
	shared_ptr<Alpha> alpha = make_shared<Alpha>(m_defaultshader);

	alpha->GetTransform()->SetPosition(_pos);
	alpha->GetTransform()->SetRotation(_rot);
	alpha->GetTransform()->SetScale(Vec3(2.f));
	CURSCENE->Add(alpha);

	return alpha;
}

void LumiaIsland::CreateCursor()
{
	auto cursorObj = make_shared<GameObject>();
	cursorObj->SetName(L"MouseCursorObject");

	m_cursor = make_shared<Cursor>();
	cursorObj->AddComponent(m_cursor);

	CURSCENE->Add(cursorObj);
}


void LumiaIsland::LoadItemBoxImages()
{
	shared_ptr<Shader> shader = make_shared<Shader>(L"ImageShader.fx");

	// 모든 UI 머티리얼에 동일한 설정 적용
	auto SetupUIMaterial = [&](shared_ptr<Material> material) {
		material->SetShader(shader);
		material->SetRenderQueue(RenderQueue::Transparent);
		material->SetTransparent(true);  // 모든 UI에 추가
		material->SetRenderingMode(RenderingMode::Forward);
		};

	wstring prefixPath = L"..\\Resources\\Textures\\UI\\ItemBox_UI\\";

	shared_ptr<Material> itemBoxPanel = make_shared<Material>();
	SetupUIMaterial(itemBoxPanel);
	auto itemBoxPanelTexture = RESOURCES->Load<Texture>(L"ItemBoxPanel", prefixPath + L"ItemBox_BackGround.png");
	itemBoxPanel->SetDiffuseMap(itemBoxPanelTexture);
	MaterialDesc& itemBoxPanelDesc = itemBoxPanel->GetMaterialDesc();
	itemBoxPanelDesc.ambient = Vec4(1.f);
	itemBoxPanelDesc.diffuse = Vec4(1.f);
	itemBoxPanelDesc.specular = Vec4(1.0f);
	RESOURCES->Add(L"ItemBoxPanel", itemBoxPanel);

	shared_ptr<Material> itemSlotCommon = make_shared<Material>();
	SetupUIMaterial(itemSlotCommon);
	auto itemSlotCommonTexture = RESOURCES->Load<Texture>(L"ItemSlotCommon", prefixPath + L"Img_Item_Slot_Common.png");
	itemSlotCommon->SetDiffuseMap(itemSlotCommonTexture);
	MaterialDesc& itemSlotCommonDesc = itemSlotCommon->GetMaterialDesc();
	itemSlotCommonDesc.ambient = Vec4(1.f);
	itemSlotCommonDesc.diffuse = Vec4(1.f);
	itemSlotCommonDesc.specular = Vec4(1.0f);
	RESOURCES->Add(L"ItemSlotCommon", itemSlotCommon);
}

void LumiaIsland::CreateItemBoxPanel()
{
	m_itemBox = make_shared<GameObject>();
	m_itemBox->SetName(L"ItemBoxPanel");

	auto panel = make_shared<UIPanel>();
	m_itemBox->AddComponent(panel);
	panel->SetDraggable(true);

	shared_ptr<Material> itemPanelBackGround = RESOURCES->Get<Material>(L"ItemBoxPanel")->Clone();
	panel->Create(Vec2(200.f, 200.f), Vec2(221, 117), Vec4(0.f), itemPanelBackGround);
	m_itemBox->SetLayerIndex(LAYER_UI);

	m_itemBoxSlots.clear();

	/*const Vec2 SLOT_SIZE(44.f, 26.f);
	const Vec2 SLOT_SPACING(8.25f, 8.5f);

	Vec2 startPos = Vec2(126.f, 205.f) - Vec2(2 * SLOT_SPACING.x * 0.5f, 4 * SLOT_SPACING.y * 0.5f);
	startPos += Vec2(SLOT_SPACING.x * 0.5f, SLOT_SPACING.y * 0.5f);*/

	const Vec2 SLOT_SIZE(45, 25);
	const Vec2 SLOT_SPACING(7, 8);
	Vec2 startPos = Vec2(33, 52);

	for (int row = 0; row < 2; ++row) {
		for (int col = 0; col < 4; ++col) {
			int slotIndex = row * 2 + col;

			auto slotObj = make_shared<GameObject>();
			slotObj->SetName(L"ItemBoxSlot_" + to_wstring(slotIndex));

			auto itemSlot = make_shared<ItemSlot>(nullptr, false);
			itemSlot->SetParentPanel(m_itemBox);
			itemSlot->SetSlotType(SLOTTYPE::INVENTORY);
			slotObj->AddComponent(itemSlot);

			Vec2 slotPos = Vec2(
				startPos.x + col * (SLOT_SIZE.x + SLOT_SPACING.x),
				startPos.y + row * (SLOT_SIZE.y + SLOT_SPACING.y)
			);
			itemSlot->CreateSlot(slotPos, SLOT_SIZE, slotIndex);

			itemSlot->OnSlotClicked += [this](int _slotIndex, SLOTTYPE _slotType) {
				OnItemBoxSlotClicked(_slotIndex, _slotType);
			};
			slotObj->GetTransform()->SetParent(m_itemBox->GetTransform());
			m_itemBoxSlots.push_back(itemSlot);	
		}
	}
	m_itemBox->SetActive(false);
	AddUIObject(m_itemBox, true);
	RegisterUIParent(m_itemBox);
}

void LumiaIsland::CheckPickedItemBox()
{
	//이번 프레임에 마우스로 누른 경우에만 함수 내부 실행. 
	if (!INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
		return;

	if (m_pickedObject != nullptr) {
		if ((m_pickedObject->GetType() == OBJECTTYPE::ITEMBOX || m_pickedObject->GetType() == OBJECTTYPE::DIEMONSTER) && m_currentItemBox != m_pickedObject) {
			SOUND->PlaySound(L"SFX/OpenSound_Tomb_01.wav", 16, 0.5f);
			m_currentItemBox = m_pickedObject;
			m_itemBox->SetActive(true);
			UpdateItemBoxSlots(m_currentItemBox);
		}
		else if ((m_pickedObject->GetType() == OBJECTTYPE::ITEMBOX || m_pickedObject->GetType() == OBJECTTYPE::DIEMONSTER) && m_currentItemBox == m_pickedObject) {
			m_itemBox->SetActive(false);
			for (auto item : m_itemBoxSlots) {
				item->ClearItem();
			}
			m_currentItemBox = nullptr;
			cout << "아이템박스 클릭해제됨3\n";
		}
	}
	else if (m_pickedObject == nullptr) {
		m_itemBox->SetActive(false);
		for (auto item : m_itemBoxSlots) {
			item->ClearItem();
		}

		m_currentItemBox = nullptr;
		cout << "아이템박스 클릭해제됨2\n";
	}
}

void LumiaIsland::OnItemBoxSlotClicked(int _slotIndex, SLOTTYPE _slotType)
{
	if (m_currentItemBox && _slotIndex >= 0 && _slotIndex < m_itemBoxSlots.size()) {
		auto slot = m_itemBoxSlots[_slotIndex];
		if (slot->GetItem() != nullptr) {
			auto itemBoxComponent = m_currentItemBox->GetComponent<ItemBox>();
			if (itemBoxComponent) {
				bool isempty = InventoryManager::GetInstance()->IsEmpty();

				if (isempty) {
					auto item = itemBoxComponent->DeleteItem(_slotIndex);
					InventoryManager::GetInstance()->PushItem(item);
					SOUND->PlaySound(L"SFX/equipmentinstall_underrare.wav", 15, 0.5f);
				}
				else {
					return;
				}
				//UI 슬롯 업데이트. 
				UpdateItemBoxSlots(m_currentItemBox);
			}
		}
	}
}

//그 아이템의 
void LumiaIsland::UpdateItemBoxSlots(shared_ptr<GameObject> _itemBoxObject)
{
	if (!_itemBoxObject)
		return;

	auto itemBoxComponent = _itemBoxObject->GetComponent<ItemBox>();
	if (!itemBoxComponent)
		return;

	//모든 슬롯을 _itemBoxObject의 것으로 업데이트. 
	auto items = itemBoxComponent->GetBoxInventory();

	for (int idx = 0; idx < m_itemBoxSlots.size(); ++idx) {
		if (idx < items.size() && items[idx] != nullptr) {
			m_itemBoxSlots[idx]->SetItem(items[idx]);
		}
		else {
			m_itemBoxSlots[idx]->SetItem(nullptr);
		}
	}
	//cout << "UpdateItemBoxSlots 완료\n";
}




DWORD __stdcall LumiaIsland::BackgroundLoadingThread(LPVOID _param)
{
	LumiaIsland* scene = static_cast<LumiaIsland*>(_param);

	
	try {
		EnterCriticalSection(&scene->m_loadingCS);


		ItemManager::GetInstance()->Initialize();
		RecipeManager::GetInstance()->Initialize();
		scene->m_uiManager->InitializeUI();

		scene->LoadItemBoxImages();
		
		LeaveCriticalSection(&scene->m_loadingCS);

		EnterCriticalSection(&scene->m_mainThreadTasksCS);
		scene->m_mainThreadTasks.push([scene]() {
	
			scene->CreateItemBoxPanel();
			
			scene->m_objectsCreated = true;
		});

		LeaveCriticalSection(&scene->m_mainThreadTasksCS);

		scene->m_loadingComplete = true;
		cout << "Succeed Background UI Loading.\n";
	}
	catch (...) {
		OutputDebugStringA("Failed Background Loading...\n");
	}

	return 0;
}

void LumiaIsland::ProcessMainThreadTasks()
{
	EnterCriticalSection(&m_mainThreadTasksCS);

	while (!m_mainThreadTasks.empty()) {
		auto task = m_mainThreadTasks.front();
		m_mainThreadTasks.pop();
		LeaveCriticalSection(&m_mainThreadTasksCS);

		task();

		EnterCriticalSection(&m_mainThreadTasksCS);
	}


	LeaveCriticalSection(&m_mainThreadTasksCS);
}



void LumiaIsland::CreateTestDecal()
{
	auto testDecalObj = make_shared<GameObject>();
	testDecalObj->AddComponent(make_shared<AABBBoxCollider>());
	testDecalObj->SetName(L"TestDecal");
	testDecalObj->SetType(OBJECTTYPE::MAP);

	// 2. SkillDecalIndicator 컴포넌트만 추가
	auto decalIndicator = make_shared<SkillDecalIndicator>();
	testDecalObj->AddComponent(decalIndicator);
	testDecalObj->GetTransform()->SetLocalPosition(Vec3(15, 25, 10));
	// 3. 간단한 설정
	decalIndicator->SetSkillDecal(SkillDecalType::CIRCLE, 5.0f);
	decalIndicator->SetColor(Vec4(1.0f, 1.0f, 1.0f, 0.6f));
	decalIndicator->SetStartPosition(Vec3(15, 20, 5));
	decalIndicator->ShowIndicator(true);

	// 4. Scene에 추가
	CURSCENE->Add(testDecalObj);
}


void LumiaIsland::CreateTestDummy()
{
	{
		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Bianca", L"Bianca2/Bianca");
		m2->ReadMaterial(L"Bianca2/Bianca");

		/*shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Nicky", L"Nicky/Nicky");
		m2->ReadMaterial(L"Nicky/Nicky");*/


		auto obj = make_shared<GameObject>();
		obj->SetName(L"TestDummy");
		obj->GetTransform()->SetLocalPosition(Vec3(10, 18, 15));
		obj->AddComponent(make_shared<AABBBoxCollider>());
		obj->GetCollider()->SetOffsetScale(Vec3(1, 1, 1));
		obj->GetTransform()->SetLocalScale(Vec3(2.f));
		obj->SetType(OBJECTTYPE::PLAYER);

		obj->AddComponent(make_shared<ModelRenderer>(m_defaultshader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}
		CURSCENE->Add(obj);
	}
}


void LumiaIsland::ControlPlayerStatus()
{
	PlayerStatus& playerStatus = m_player->GetStatus();
	if (INPUT->GetButton(KEY_TYPE::KEY_1))
	{
		m_player->SetHitAttack(playerStatus.hitAttack + 1);
	}
	if (INPUT->GetButton(KEY_TYPE::KEY_2))
	{
		m_player->SetDefense(playerStatus.defense + 1);
	}
	if (INPUT->GetButton(KEY_TYPE::KEY_3))
	{
		m_player->SetHitSpeed(playerStatus.hitSpeed + 0.01f);
	}
	if (INPUT->GetButton(KEY_TYPE::KEY_4))
	{
		m_player->SetCooldownReduction(playerStatus.cooldownReduction + 10);
	}
	if (INPUT->GetButton(KEY_TYPE::KEY_5))
	{
		m_player->SetMoveSpeed(playerStatus.moveSpeed + 0.01);
	}
	if (INPUT->GetButtonDown(KEY_TYPE::Z))
	{
		m_player->SetHP(playerStatus.hp -= 10);
	}
	if (INPUT->GetButtonDown(KEY_TYPE::C))
	{
		m_player->SetStamina(playerStatus.stamina -= 10);
	}
	if (INPUT->GetButtonDown(KEY_TYPE::B))
	{
		int qLevel = m_player->GetSkill(0)->GetCurSkillLevel();
		int wLevel = m_player->GetSkill(1)->GetCurSkillLevel();
		int eLevel = m_player->GetSkill(2)->GetCurSkillLevel();
		int rLevel = m_player->GetSkill(3)->GetCurSkillLevel();


		cout << "스킬 레벨 : " << qLevel << " , " << wLevel << " , " << eLevel << " , " << rLevel << endl;
	}
	if (INPUT->GetButtonDown(KEY_TYPE::D))
	{
		InventoryManager::GetInstance()->PushItem(ItemManager::GetInstance()->GetItem(L"피아노선"));
	}
}

// LumiaIsland.cpp에 구현 추가
bool LumiaIsland::IsCraftStateCompleted()
{
	// 현재 Player의 PlayerState가 Craft인지 확인하고, 완료 상태인지 체크
	auto psm = m_player->GetPlayerStateMachine();
	if (psm && psm->IsInState(PlayerStateType::Craft))
	{
		auto currentState = psm->GetCurrentStatePtr();
		if (currentState)
		{
			if (m_selectedCharacterIdx == 1)
			{
				// NickyCraftState로 캐스팅해서 완료 상태 확인
				auto craftState = dynamic_pointer_cast<NickyCraftState>(currentState);
				if (craftState)
				{
					// NickyCraftState의 private 멤버에 접근하기 위해 friend 선언 필요하거나
					// public getter 메서드 추가 필요
					return craftState->IsSkillComplete(); // 이 메서드를 NickyCraftState에 추가 필요
				}
			}
			else if (m_selectedCharacterIdx == 0)
			{
				// BiancaCraftState로 캐스팅해서 완료 상태 확인
				auto craftState = dynamic_pointer_cast<BiancaCraftState>(currentState);
				if (craftState)
				{
					// BiancaCraftState의 private 멤버에 접근하기 위해 friend 선언 필요하거나
					// public getter 메서드 추가 필요
					return craftState->IsSkillComplete(); // 이 메서드를 BiancaCraftState에 추가 필요
				}
			}


			
		}
	}
	return false;
}

// LumiaIsland.cpp에 구현 추가
bool LumiaIsland::IsQSkillCompleted()
{
	// 현재 Player의 PlayerState가 Skill_1인지 확인하고, 완료 상태인지 체크
	auto psm = m_player->GetPlayerStateMachine();
	if (psm && psm->IsInState(PlayerStateType::Skill_1))
	{
		auto currentState = psm->GetCurrentStatePtr();
		if (currentState)
		{
			// 니키인지 비앙카인지 확인 후 적절한 완료 체크
			if (m_selectedCharacterIdx == 1) // 니키
			{
				auto qState = dynamic_pointer_cast<NickyQState>(currentState);
				if (qState)
				{
					return qState->IsSkillComplete(); // 이 메서드를 NickyQState에 추가 필요
				}
			}
			else if (m_selectedCharacterIdx == 0) // 비앙카
			{
				auto qState = dynamic_pointer_cast<BiancaQState>(currentState);
				if (qState)
				{
					return qState->IsSkillComplete(); // 이 메서드를 BiancaQState에 추가 필요
				}
			}
		}
	}
	return false;
}

// LumiaIsland.cpp에 구현 추가
bool LumiaIsland::IsWSkillCompleted()
{
	// 현재 Player의 PlayerState가 Skill_2인지 확인하고, 완료 상태인지 체크
	auto psm = m_player->GetPlayerStateMachine();
	if (psm && psm->IsInState(PlayerStateType::Skill_2))
	{
		auto currentState = psm->GetCurrentStatePtr();
		if (currentState)
		{
			// 니키인지 비앙카인지 확인 후 적절한 완료 체크
			if (m_selectedCharacterIdx == 1) // 니키
			{
				auto wState = dynamic_pointer_cast<NickyWState>(currentState);
				if (wState)
				{
					return wState->IsSkillComplete(); // 이 메서드를 NickyQState에 추가 필요
				}
			}
			else if (m_selectedCharacterIdx == 0) // 비앙카
			{
				auto wState = dynamic_pointer_cast<BiancaWState>(currentState);
				if (wState)
				{
					return wState->IsSkillComplete(); // 이 메서드를 BiancaQState에 추가 필요
				}
			}
		}
	}
	return false;
}

// LumiaIsland.cpp에 구현 추가
bool LumiaIsland::IsESkillCompleted()
{
	// 현재 Player의 PlayerState가 Skill_1인지 확인하고, 완료 상태인지 체크
	auto psm = m_player->GetPlayerStateMachine();
	if (psm && psm->IsInState(PlayerStateType::Skill_3))
	{
		auto currentState = psm->GetCurrentStatePtr();
		if (currentState)
		{
			// 니키인지 비앙카인지 확인 후 적절한 완료 체크
			if (m_selectedCharacterIdx == 1) // 니키
			{
				auto eState = dynamic_pointer_cast<NickyEState>(currentState);
				if (eState)
				{
					return eState->IsSkillComplete(); // 이 메서드를 NickyEState에 추가 필요
				}
			}
			else if (m_selectedCharacterIdx == 0) // 비앙카
			{
				auto eState = dynamic_pointer_cast<BiancaEState>(currentState);
				if (eState)
				{
					return eState->IsSkillComplete(); // 이 메서드를 BiancaEState에 추가 필요
				}
			}
		}
	}
	return false;
}

bool LumiaIsland::IsRSkillCompleted()
{
	// 현재 Player의 PlayerState가 Skill_1인지 확인하고, 완료 상태인지 체크
	auto psm = m_player->GetPlayerStateMachine();
	if (psm && psm->IsInState(PlayerStateType::Skill_4))
	{
		auto currentState = psm->GetCurrentStatePtr();
		if (currentState)
		{
			// 니키인지 비앙카인지 확인 후 적절한 완료 체크
			if (m_selectedCharacterIdx == 1) // 니키
			{
				auto rState = dynamic_pointer_cast<NickyRState>(currentState);
				if (rState)
				{
					return rState->IsSkillComplete(); // 이 메서드를 NickyEState에 추가 필요
				}
			}
			else if (m_selectedCharacterIdx == 0) // 비앙카
			{
				auto rState = dynamic_pointer_cast<BiancaRState>(currentState);
				if (rState)
				{
					return rState->IsSkillComplete(); // 이 메서드를 BiancaEState에 추가 필요
				}
			}
		}
	}
	return false;
}

bool LumiaIsland::IsCounterSkillCompleted()
{
	// 현재 Player의 PlayerState가 counter인지 확인하고, 완료 상태인지 체크
	auto psm = m_player->GetPlayerStateMachine();
	if (psm && psm->IsInState(PlayerStateType::Counter))
	{
		auto currentState = psm->GetCurrentStatePtr();
		if (currentState)
		{
			// 니키인지 비앙카인지 확인 후 적절한 완료 체크
			if (m_selectedCharacterIdx == 1) // 니키
			{
				auto counterState = dynamic_pointer_cast<NickyCounterState>(currentState);
				if (counterState)
				{
					return counterState->IsSkillComplete(); // 이 메서드를 NickyEState에 추가 필요
				}
			}
			else if (m_selectedCharacterIdx == 0) // 비앙카
			{
				
			}
		}
	}
	return false;
}

// 새로운 메서드 추가
void LumiaIsland::HandleSkillLevelUpInput()
{
	if (!m_player) return;

	// CTRL이 눌린 상태에서만 처리
	if (INPUT->GetButton(KEY_TYPE::LCTRL))
	{
		if (INPUT->GetButtonDown(KEY_TYPE::Q))
		{
			LevelUpSkill(0); // Q 스킬
		}
		else if (INPUT->GetButtonDown(KEY_TYPE::W))
		{
			LevelUpSkill(1); // W 스킬
		}
		else if (INPUT->GetButtonDown(KEY_TYPE::E))
		{
			LevelUpSkill(2); // E 스킬
		}
		else if (INPUT->GetButtonDown(KEY_TYPE::R))
		{
			LevelUpSkill(3); // R 스킬
		}
	}
}

void LumiaIsland::LevelUpSkill(int skillIndex)
{
	if (!m_player) return;

	PlayerStatus& playerStatus = m_player->GetStatus();

	// 스킬포인트가 있는지 확인
	if (playerStatus.availableSkillPoints <= 0)
	{
		cout << "사용 가능한 스킬포인트가 없습니다." << endl;
		return;
	}

	ISkill* skill = m_player->GetSkill(skillIndex);
	if (!skill) return;

	int curLevel = skill->GetCurSkillLevel();
	int maxLevel = skill->GetMaxSkillLevel();

	// 최대 레벨인지 확인
	if (curLevel >= maxLevel)
	{
		cout << "스킬이 이미 최대 레벨입니다." << endl;
		return;
	}

	// 스킬 레벨업 실행
	skill->SkillLevelUp();
	playerStatus.availableSkillPoints--;

	// UI 업데이트
	if (m_uiManager)
	{
		m_uiManager->GetGameHUD()->UpdateSkillLevelBar(skillIndex);
	}

	

	cout << "스킬 " << (char)('Q' + skillIndex) << " 레벨업! 현재 레벨: "
		<< skill->GetCurSkillLevel() << "/" << maxLevel << endl;
}