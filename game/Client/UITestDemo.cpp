#include "pch.h"
#include "UITestDemo.h"
#include "GeometryHelper.h"
#include "Camera.h"
#include "Scene.h"
#include "GameObject.h"
#include "MeshRenderer.h"
#include "Mesh.h"
#include "Material.h"
#include "Model.h"
#include "ModelRenderer.h"
#include "ModelAnimator.h"
#include "Transform.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Light.h"
#include "Rigidbody.h"
#include "AABBBoxCollider.h"
#include "OBBBoxCollider.h"
#include "SphereCollider.h"
#include "Billboard.h"
#include "Terrain.h"
#include "Sky.h"
#include "Button.h"
#include "Text.h"

#include "UIPanel.h"
#include "ImageUI.h"
#include "CameraScript.h"

#include "AnimationStateMachine.h"

#include "NickyWaitState.h"
#include "NickyRunState.h"
#include "NickyESkillState.h"
#include "NickyWSkillState.h"
#include "NickyQSkillState.h"
#include "NickyRSkillState.h"

#include "BiancaRunState.h"
#include "BiancaQSkillState.h"
#include "BiancaWaitState.h"
#include "BiancaESkillState.h"
#include "BiancaRSkillState.h"


void UITestDemo::Init()
{	
//	CURSCENE->SetSky(make_shared<Sky>(L"..\\Resources\\Textures\\Sky\\snowcube1024.dds", L"Sky.fx"));
	//shared_ptr<Shader> renderShader = make_shared<Shader>(L"23. RenderDemo.fx");
	shared_ptr<Shader> renderShader = make_shared<Shader>(L"FOW.fx");
	shared_ptr<Shader> imageShader = make_shared<Shader>(L"ImageShader.fx");

	{
		// Camera
		auto camera = make_shared<GameObject>();
		camera->SetName(L"MainCamera");
		camera->GetTransform()->SetPosition(Vec3{ 0.f, 0.f, -5.f });
		camera->AddComponent(make_shared<Camera>());
		camera->AddComponent(make_shared<CameraScript>());
		camera->GetCamera()->SetNear(1.f);
		camera->GetCamera()->SetFar(500.f);
		camera->GetCamera()->SetProjectionType(ProjectionType::Perspective);
		camera->GetCamera()->SetCullingMaskLayerOnOff(LAYER_UI, true);
		CURSCENE->Add(camera);
	
	}

	{
		// Light
		auto light = make_shared<GameObject>();
		light->SetName(L"Light");
		light->AddComponent(make_shared<Light>());

		LightDesc lightDesc;
		lightDesc.ambient = Vec4(0.4f);
		lightDesc.diffuse = Vec4(1.f);
		lightDesc.specular = Vec4(0.1f);
		lightDesc.direction = Vec3(1.f, -1.f, 1.f);
		//light->GetLight()->SetLightDesc(lightDesc);
		light->GetTransform()->SetRotation(lightDesc.direction);
		light->GetTransform()->SetPosition(Vec3(0.f, -150.f, 0.f));
		static_pointer_cast<Light>(light->GetFixedComponent(ComponentType::Light))->SetLightDesc(lightDesc);
		CURSCENE->Add(light);
	}


	{
		// Animation
		shared_ptr<Model> m1 = make_shared<Model>();

		m1->ReadModel(L"Nicky/Nicky");
		m1->ReadMaterial(L"Nicky/Nicky");



		//대기
		m1->ReadAnimation(L"Wait", L"Nicky/Nicky_Glove_Wait");
		
		//달리기
		m1->ReadAnimation(L"Run", L"Nicky/Nicky_Glove_Run");

		//평타
		m1->ReadAnimation(L"BaseAttack_01", L"Nicky/Nicky_Glove_Atk_01");
		m1->ReadAnimation(L"BaseAttack_02", L"Nicky/Nicky_Glove_Atk_02");

		////Q
		m1->ReadAnimation(L"Skill_01_Attack", L"Nicky/Nicky_Glove_Skill_01_Attack");
		m1->ReadAnimation(L"Skill_01_Rush", L"Nicky/Nicky_Glove_Skill_01_Rush");
		m1->ReadAnimation(L"Skill_01_End", L"Nicky/Nicky_Glove_Skill_01_End");
		//Q Charge
		m1->ReadAnimation(L"Skill_01_Charge_Loop_Run", L"Nicky/Nicky_Glove_Skill_01_Charge_Loop_Run");
		m1->ReadAnimation(L"Skill_01_Charge_Start_Run", L"Nicky/Nicky_Glove_Skill_01_Charge_Start_Run");
		m1->ReadAnimation(L"Skill_01_Charge_Loop_Wait", L"Nicky/Nicky_Glove_Skill_01_Charge_Loop_Wait");
		m1->ReadAnimation(L"Skill_01_Charge_Start_Wait", L"Nicky/Nicky_Glove_Skill_01_Charge_Start_Wait");

		//W
		m1->ReadAnimation(L"Skill_02_Guard", L"Nicky/Nicky_Glove_Skill_02_Guard");
		m1->ReadAnimation(L"Skill_02_Loop", L"Nicky/Nicky_Glove_Skill_02_Loop");
	
		//E
		m1->ReadAnimation(L"Skill_03", L"Nicky/Nicky_Glove_Skill_03");

		//R
		m1->ReadAnimation(L"Skill_04_Attack", L"Nicky/Nicky_Glove_Skill_04_Attack");
		m1->ReadAnimation(L"Skill_04_Ready", L"Nicky/Nicky_Glove_Skill_04_Ready");
		m1->ReadAnimation(L"Skill_04_Start", L"Nicky/Nicky_Glove_Skill_04_Start");

		

		for (int32 i = 0; i < 1; i++)
		{

			nicky = make_shared<GameObject>();
			nicky->SetName(to_wstring(i));

			nicky->GetTransform()->SetPosition(Vec3(0, 0, 0));
			nicky->GetTransform()->SetScale(Vec3(1.f));

			nicky->AddComponent(make_shared<SphereCollider>());
			nicky->AddComponent(make_shared<Rigidbody>());
			nicky->GetCollider()->SetOffset(Vec3(0.f, 1.f, 0.f));
			nicky->GetRigidbody()->SetStatic(true);

		/*	nicky->AddComponent(make_shared<ModelAnimator>(renderShader));
			{
				nicky->GetModelAnimator()->SetModel(m1);
				nicky->GetModelAnimator()->SetPass(2);
			}*/

			nicky->AddComponent(make_shared<ModelRenderer>(renderShader));
			{
				nicky->GetModelRenderer()->SetModel(m1);
				nicky->GetModelRenderer()->SetPass(3);
			}

			//// FSM 추가
			//auto stateMachine = make_shared<AnimationStateMachine>();
			//nicky->AddComponent(stateMachine);

			//nicky->GetAnimationStateMachine()->RegisterState(AnimationStateType::Wait, make_shared<NickyWaitState>());
			//nicky->GetAnimationStateMachine()->RegisterState(AnimationStateType::Run, make_shared<NickyRunState>());

			//nicky->GetAnimationStateMachine()->RegisterState(AnimationStateType::Skill_2, make_shared<NickyWSkillState>());
			//nicky->GetAnimationStateMachine()->RegisterState(AnimationStateType::Skill_3, make_shared<NickyESkillState>());
			//nicky->GetAnimationStateMachine()->RegisterState(AnimationStateType::Skill_4, make_shared<NickyRSkillState>());
			//nicky->GetAnimationStateMachine()->RegisterState(AnimationStateType::Skill_1, make_shared<NickyQSkillState>());


			//// 기존 시퀀스 생성 코드는 유지 (필요시 사용)
			//auto animator = nicky->GetModelAnimator();
			//// E 스킬 시퀀스 (Skill_03 단일)
			//vector<wstring> skill3Anims = { L"Skill_03" };
			//animator->CreateSequence(L"Skill_3_Sequence", skill3Anims, false);
		
			//// 평타 시퀀스 (BaseAttack_01 -> BaseAttack_02)
			//vector<wstring> baseAttackAnims = { L"BaseAttack_02", L"BaseAttack_01" };
			//vector<float> baseAttackDurations = { 0.8f, 1.2f }; 
			//animator->CreateSequence(L"BaseAttack_Sequence", baseAttackAnims, baseAttackDurations, false);

			//// Q 스킬 시퀀스 (Skill_01_Attack -> Skill_01_Rush -> Skill_01_End)
			//vector<wstring> skill1Anims = { L"Skill_01_Attack", L"Skill_01_Rush", L"Skill_01_End" };
			//vector<float> skill1Durations = { 0.5f, 1.0f, 0.7f }; 
			//animator->CreateSequence(L"Skill_1_Sequence", skill1Anims, skill1Durations, false);

			//// W 스킬 시퀀스 (Skill_02_Guard -> Skill_02_Loop)
			//vector<wstring> skill2Anims = { L"Skill_02_Guard" };
			//animator->CreateSequence(L"Skill_2_Sequence", skill2Anims, false);

			////// E 스킬 시퀀스 (Skill_03 단일)
			////vector<wstring> skill3Anims = { L"Skill_03" };
			////animator->CreateSequence(L"Skill_3_Sequence", skill3Anims, false);

			//// R 스킬 시퀀스 (Skill_04_Ready -> Skill_04_Start -> Skill_04_Attack)
			//vector<wstring> skill4Anims = { L"Skill_04_Ready", L"Skill_04_Start", L"Skill_01_Rush", L"Skill_04_Attack"};
			//vector<float> skill4Durations; 
			//skill4Durations.push_back(animator->GetAnimationDuration(L"Skill_04_Ready"));  
			//skill4Durations.push_back(animator->GetAnimationDuration(L"Skill_04_Start"));  
			//skill4Durations.push_back(3.f); 
			//skill4Durations.push_back(animator->GetAnimationDuration(L"Skill_04_Attack"));  
			//animator->CreateSequence(L"Skill_4_Sequence", skill4Anims, skill4Durations, false);
			
			

			
			CURSCENE->Add(nicky);
		}


		//// 여러 클러스터로 나누어 배치
		//for (int32 cluster = 0; cluster < 5; cluster++)
		//{
		//	Vec3 clusterCenter = Vec3(
		//		(cluster % 3 - 1) * 500,  // -200, 0, 200
		//		0,
		//		(cluster / 3 - 1) * 500   // -200, 0, 200
		//	);

		//	for (int32 i = 0; i < 10; i++)
		//	{
		//		auto obj = make_shared<GameObject>();
		//		obj->SetName(to_wstring(cluster * 100 + i));

		//		// 클러스터 중심 주변에 배치
		//		obj->GetTransform()->SetPosition(clusterCenter + Vec3(
		//			(rand() % 40) - 20,  // 클러스터 내 랜덤
		//			0,
		//			(rand() % 40) - 20
		//		));

		//		// 나머지 코드...
		//		obj->GetTransform()->SetScale(Vec3(1.f));

		//		obj->AddComponent(make_shared<SphereCollider>());
		//		obj->AddComponent(make_shared<Rigidbody>());
		//		obj->GetCollider()->SetOffset(Vec3(0.f, 1.f, 0.f));
		//		obj->GetRigidbody()->SetStatic(true);

		//		/*obj->AddComponent(make_shared<ModelRenderer>(renderShader));
		//		{
		//			obj->GetModelRenderer()->SetModel(m1);
		//			obj->GetModelRenderer()->SetPass(1);
		//		}*/

		//		obj->AddComponent(make_shared<ModelAnimator>(renderShader));
		//		{
		//			obj->GetModelAnimator()->SetModel(m1);
		//			obj->GetModelAnimator()->SetPass(2);
		//		}

		//		CURSCENE->Add(obj);
		//	}
		//}
	}
	
	
	
	
	//{
	//	// Animation
	//	shared_ptr<Model> m1 = make_shared<Model>();

	//	m1->ReadModel(L"Bianca2/Bianca");
	//	m1->ReadMaterial(L"Bianca2/Bianca");
	//	m1->ReadAnimation(L"Wait", L"Bianca2/Bianca_wait");
	//	m1->ReadAnimation(L"Run", L"Bianca2/Bianca_run");

	//	m1->ReadAnimation(L"Skill_1", L"Bianca2/Bianca_skill1");

	//	m1->ReadAnimation(L"Skill_3_1", L"Bianca2/Bianca_skill3-1");
	//	m1->ReadAnimation(L"Skill_3_2", L"Bianca2/Bianca_skill3-2");
	//	m1->ReadAnimation(L"Skill_3_3", L"Bianca2/Bianca_skill3-3");


	//	m1->ReadAnimation(L"Skill_4_1", L"Bianca2/Bianca_skill4");
	//	m1->ReadAnimation(L"Skill_4_2", L"Bianca2/Bianca_skill4-2");



	//	for (int32 i = 0; i < 1; i++)
	//	{

	//		auto obj = make_shared<GameObject>();
	//		obj->GetTransform()->SetPosition(Vec3(0.f, 0.f, 0.f));
	//		obj->GetTransform()->SetScale(Vec3(1.f));

	//		obj->AddComponent(make_shared<ModelAnimator>(renderShader));
	//		{
	//			obj->GetModelAnimator()->SetModel(m1);
	//			obj->GetModelAnimator()->SetPass(2);
	//		}
	//		obj->AddComponent(make_shared<AABBBoxCollider>());
	//	
	//	
	//		auto animator = obj->GetModelAnimator();
	//		// FSM 추가
	//		auto stateMachine = make_shared<AnimationStateMachine>();
	//		obj->AddComponent(stateMachine);

	//		obj->GetAnimationStateMachine()->RegisterState(AnimationStateType::Wait, make_shared<BiancaWaitState>());
	//		obj->GetAnimationStateMachine()->RegisterState(AnimationStateType::Run, make_shared<BiancaRunState>());
	//		obj->GetAnimationStateMachine()->RegisterState(AnimationStateType::Skill_1, make_shared<BiancaQSkillState>());
	//		obj->GetAnimationStateMachine()->RegisterState(AnimationStateType::Skill_3, make_shared<BiancaESkillState>());
	//		obj->GetAnimationStateMachine()->RegisterState(AnimationStateType::Skill_4, make_shared<BiancaRSkillState>());

	//		// Q 스킬 시퀀스 
	//		vector<wstring> skill1Anims = { L"Skill_1" };
	//		animator->CreateSequence(L"Skill_1_Sequence", skill1Anims, false);

	//		// R 스킬 시퀀스 (Skill_04_Ready -> Skill_04_Start -> Skill_04_Attack)
	//		vector<wstring> skill4Anims = { L"Skill_4_1", L"Skill_4_2" };
	//		animator->CreateSequence(L"Skill_4_Sequence", skill4Anims, false);

	//		CURSCENE->Add(obj);

	//		//camera->GetTransform()->SetParent(obj->GetTransform());
	//		//auto BiancaCam = make_shared<BiancaCamera>();
	//		//camera->AddComponent(BiancaCam);
	//		//BiancaCam->SetTarget(obj);
	//		//BiancaCam->SetOffset(Vec3(0.f, 12.f, -12.5f));
	//		//camera->GetTransform()->SetRotation(Vec3{ 45.f, 0.f, 0.f });

	//	}


	//	//// 여러 클러스터로 나누어 배치
	//	//for (int32 cluster = 0; cluster < 5; cluster++)
	//	//{
	//	//	Vec3 clusterCenter = Vec3(
	//	//		(cluster % 3 - 1) * 500,  // -200, 0, 200
	//	//		0,
	//	//		(cluster / 3 - 1) * 500   // -200, 0, 200
	//	//	);

	//	//	for (int32 i = 0; i < 10; i++)
	//	//	{
	//	//		auto obj = make_shared<GameObject>();
	//	//		obj->SetName(to_wstring(cluster * 100 + i));

	//	//		// 클러스터 중심 주변에 배치
	//	//		obj->GetTransform()->SetPosition(clusterCenter + Vec3(
	//	//			(rand() % 40) - 20,  // 클러스터 내 랜덤
	//	//			0,
	//	//			(rand() % 40) - 20
	//	//		));

	//	//		// 나머지 코드...
	//	//		obj->GetTransform()->SetScale(Vec3(1.f));

	//	//		obj->AddComponent(make_shared<SphereCollider>());
	//	//		obj->AddComponent(make_shared<Rigidbody>());
	//	//		obj->GetCollider()->SetOffset(Vec3(0.f, 1.f, 0.f));
	//	//		obj->GetRigidbody()->SetStatic(true);

	//	//		/*obj->AddComponent(make_shared<ModelRenderer>(renderShader));
	//	//		{
	//	//			obj->GetModelRenderer()->SetModel(m1);
	//	//			obj->GetModelRenderer()->SetPass(1);
	//	//		}*/

	//	//		obj->AddComponent(make_shared<ModelAnimator>(renderShader));
	//	//		{
	//	//			obj->GetModelAnimator()->SetModel(m1);
	//	//			obj->GetModelAnimator()->SetPass(2);
	//	//		}

	//	//		CURSCENE->Add(obj);
	//	//	}
	//	//}
	//}
	
	// UI
	{
		// Material
		
		shared_ptr<Material> material = make_shared<Material>();
		material->SetShader(imageShader);
		auto texture = RESOURCES->Load<Texture>(L"BtnImg", L"..\\Resources\\Textures\\UI_Btn\\Img_Item_Slot_Legendary.png");
		material->SetDiffuseMap(texture);
		MaterialDesc& desc = material->GetMaterialDesc();
		desc.ambient = Vec4(1.f);
		desc.diffuse = Vec4(1.f);
		desc.specular = Vec4(1.f);
		RESOURCES->Add(L"BtnImg", material);

		shared_ptr<Material> panelMaterial = make_shared<Material>();
		panelMaterial->SetShader(renderShader);
		auto texture2 = RESOURCES->Load<Texture>(L"PanelImg", L"..\\Resources\\Textures\\UI_Btn\\Img_Item_Slot_Common_MouseOver.png");
		panelMaterial->SetDiffuseMap(texture2);
		MaterialDesc& panelDesc = panelMaterial->GetMaterialDesc();
		panelDesc.ambient = Vec4(0.3f, 0.3f, 0.3f, 0.9f);
		panelDesc.diffuse = Vec4(0.3f, 0.3f, 0.3f, 0.9f);
		panelDesc.specular = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		RESOURCES->Add(L"PanelImg", panelMaterial);

		shared_ptr<Material> nicky = make_shared<Material>();
		nicky->SetShader(imageShader);
		auto texture3 = RESOURCES->Load<Texture>(L"NickyImg", L"..\\Resources\\Textures\\UI_Select\\CharLobby_Nicky_S000.png");
		nicky->SetDiffuseMap(texture3);
		MaterialDesc& nickyDesc = nicky->GetMaterialDesc();
		nickyDesc.ambient = Vec4(1.f);
		nickyDesc.diffuse = Vec4(1.f);
		nickyDesc.specular = Vec4(1.0f);
		RESOURCES->Add(L"NickyImg", nicky);

		shared_ptr<Material> backGround = make_shared<Material>();
		backGround->SetShader(imageShader);
		auto texture4 = RESOURCES->Load<Texture>(L"BackImg", L"..\\Resources\\Textures\\UI_Select\\Img_Slot_Character_Route.png");
		backGround->SetDiffuseMap(texture4);
		MaterialDesc& backGroundDesc = backGround->GetMaterialDesc();
		backGroundDesc.ambient = Vec4(1.f);
		backGroundDesc.diffuse = Vec4(1.f);
		backGroundDesc.specular = Vec4(1.0f);
		RESOURCES->Add(L"BackImg", backGround);



		CreatePanelWithImageUI();

	
		//
		//// UIPanel GameObject 생성
		//auto panelObj = make_shared<GameObject>();
		//panelObj->SetName(L"MainUIPanel");

		//// UIPanel 컴포넌트 추가
		//auto uiPanel = make_shared<UIPanel>();
		//panelObj->AddComponent(uiPanel);

		//// 패널 생성 (화면 중앙에 300x200 크기)
		//uiPanel->Create(Vec2(600, 400), Vec2(800, 600), nullptr);
		//
		//// 패널 내부에 텍스트들 추가
		//auto titleText = uiPanel->AddText(
		//	Vec2(0, 0),                          // 패널 내 로컬 위치
		//	L"게임 메뉴",                           // 텍스트 내용
		//	50.0f,                                  // 폰트 크기
		//	Vec4(1.0f, 1.0f, 1.0f, 1.0f),          // 흰색 글자
		//	1.0f,                                   // 투명도
		//	Vec4(0.0f, 0.0f, 0.0f, 1.0f),          // 검은색 외곽선
		//	2.0f,                                   // 외곽선 두께
		//	L"TitleText"                            // 텍스트 이름
		//);

		//// 씬에 추가
		//CURSCENE->AddUIObject(panelObj);


		
		
		
		

	
		//// Button GameObject (부모)
		//{
		//	auto buttonObj = make_shared<GameObject>();
		//	buttonObj->SetName(L"UI_Button");

		//	// Button 컴포넌트 추가
		//	buttonObj->AddComponent(make_shared<Button>());
		//	buttonObj->GetButton()->Create(Vec2(100, 100), Vec2(100, 100), RESOURCES->Get<Material>(L"BtnImg"));


		//	buttonObj->GetButton()->AddOnClickedEvent([buttonObj]() {
		//		std::wcout << buttonObj->GetName() << " : clicked\n";
		//		});

		//	buttonObj->SetLayerIndex(LAYER_UI);
		//	CURSCENE->Add(buttonObj);
		//}

		//// Text GameObject (별도 객체)
		//{
		//	auto textObj = make_shared<GameObject>();
		//	textObj->SetName(L"UI_Text");

		//	// Text 컴포넌트 추가
		//	shared_ptr<Text> textComponent = make_shared<Text>();
		//	textObj->AddComponent(textComponent);

		//	textComponent->Create(
		//		Vec2(300, 400),                    // Button과 같은 위치
		//		L"버튼 텍스트",                     // 텍스트
		//		20.0f,                            // 크기
		//		Vec4(1.0f, 1.0f, 1.0f, 1.0f),     // 흰색 글자
		//		1.0f,                             // 투명도
		//		Vec4(0.0f, 0.0f, 0.0f, 1.0f),     // 검은색 외곽선
		//		2.0f                              // 외곽선 두께
		//	);

		//	textObj->SetLayerIndex(LAYER_UI);
		//	CURSCENE->Add(textObj);
		//}
	}

	{
		
		// UICamera
		auto camera = make_shared<GameObject>();
		camera->SetName(L"UICamera");
		camera->GetTransform()->SetPosition(Vec3{ 0.f, 0.f, -5.f });
		camera->AddComponent(make_shared<Camera>());
		camera->GetCamera()->SetProjectionType(ProjectionType::Orthographic);
		camera->GetCamera()->SetNear(1.0f);
		camera->GetCamera()->SetFar(100.0f);
		camera->GetCamera()->SetCullingMaskAll();
		camera->GetCamera()->SetCullingMaskLayerOnOff(LAYER_UI, false);
		CURSCENE->Add(camera);
		
	}
	CURSCENE->Start();
}

void UITestDemo::Update()
{
	if (INPUT->GetButtonDown(KEY_TYPE::RBUTTON))
	{// 또는 명시적으로
		//CURSCENE->GetObjectManager()->MarkUIObjectForDestroyWithChildren(panelObj);
		CURSCENE->Remove(nicky);
	}	
}

void UITestDemo::Render()
{
}

void UITestDemo::ShowImguiTransform()
{

}

// UIPanel에 ImageUI 추가 사용 예제
void UITestDemo::CreatePanelWithImageUI()
{
	// UIPanel GameObject 생성
	{
		panelObj = make_shared<GameObject>();
		panelObj->SetName(L"UI_Panel2");

		// UIPanel 컴포넌트 추가
		panelObj->AddComponent(make_shared<UIPanel>());
		panelObj->GetUIPanel()->Create(Vec2(400, 300), Vec2(600, 400), Vec4(0.f),
			nullptr);

		// 패널에 ImageUI 추가
		auto imageUI = panelObj->GetUIPanel()->AddImageUI(Vec2(0, 0), L"MainImageUI");

		// ImageUI에 이미지 레이어들 추가
		imageUI->AddImageLayer(0, Vec2(400, 200), Vec2(106, 166),
			RESOURCES->Get<Material>(L"BackImg"), 1);

		imageUI->AddImageLayer(10, Vec2(400, 200), Vec2(122, 158),
			RESOURCES->Get<Material>(L"NickyImg"), 1);

		// 패널에 버튼도 추가 가능
		auto button = panelObj->GetUIPanel()->AddButton(Vec2(100, 100), Vec2(80, 40),
			RESOURCES->Get<Material>(L"BtnImg"), L"TestButton");

		button->AddOnClickedEvent([button]() {
				std::wcout << button->GetGameObject()->GetName() << " : clicked\n";
				});
		
		panelObj->GetUIPanel()->AddText(
			Vec2(0.f),
			L"Test",
			20.f,
			Color(1.f, 0.f, 0.f, 1.f),
			1.f,
			Color(0.f, 0.f, 0.f, 1.f),
			2.0f,
			L"TitleText"
		);
		
		panelObj->SetLayerIndex(LAYER_UI);
		// **UI 객체로 씬에 추가 (부모로 등록)**
		CURSCENE->AddUIObject(panelObj, true);  // true = 부모
		CURSCENE->RegisterUIParent(panelObj);
	}
}