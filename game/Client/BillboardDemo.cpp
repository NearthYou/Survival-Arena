#include "pch.h"
#include "BillboardDemo.h"
#include "BiancaTest.h"
#include "BiancaCamera.h"
#include "FogOfWar.h"
#include "CameraScript.h"

#include "AnimationStateMachine.h"
#include "BiancaRunState.h"



void BillboardDemo::Init()
{
	CURSCENE->SetSky(make_shared<Sky>(L"..\\Resources\\Textures\\Sky\\snowcube1024.dds", L"Sky.fx"));
	shared_ptr<Shader> renderShader = make_shared<Shader>(L"FOW.fx");
	shared_ptr<Shader> grassShader = make_shared<Shader>(L"28. BillboardDemo.fx");
	// Camera
	auto camera = make_shared<GameObject>();
	camera->AddComponent(make_shared<Camera>());
	camera->AddComponent(make_shared<CameraScript>());
	//camera->AddComponent(make_shared<FogOfWar>());

	camera->GetCamera()->SetCullingMaskLayerOnOff(LAYER_UI, true);
	CURSCENE->Add(camera);
	
	{
		// UICamera
		auto camera = make_shared<GameObject>();
		camera->GetTransform()->SetPosition(Vec3{ 0.f, 0.f, -5.f });
		camera->AddComponent(make_shared<Camera>());
		camera->GetCamera()->SetProjectionType(ProjectionType::Orthographic);
		camera->GetCamera()->SetNear(1.0f);
		camera->GetCamera()->SetFar(100.0f);
		camera->GetCamera()->SetCullingMaskAll();
		camera->GetCamera()->SetCullingMaskLayerOnOff(LAYER_UI, false);
		CURSCENE->Add(camera);
	}

	{

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Cemetery_STR_Base", L"Cemetery/Cemetery_STR_Base");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Cemetery/Cemetery_STR_Base");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Cemetery_STR_Base");
			obj->GetTransform()->SetLocalPosition(Vec3(0, -10, 0));
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(renderShader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}

			CURSCENE->Add(obj);
		}

		float movex = -50.f;
		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Common_OBJ_Lavender_01", L"Environment/Common_OBJ_Lavender_01");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Environment/Common_OBJ_Lavender_01");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Common_OBJ_Lavender_01");
			obj->GetTransform()->SetLocalPosition(Vec3(movex, 0, 0));
			movex += 5.f;
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(renderShader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}

			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Common_OBJ_Longgrass_01", L"Environment/Common_OBJ_Longgrass_01");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Environment/Common_OBJ_Longgrass_01");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Common_OBJ_Longgrass_01");
			obj->GetTransform()->SetLocalPosition(Vec3(movex, 0, 0));
			movex += 5.f;
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(renderShader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}

			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"NATURE_FERN_00", L"Environment/NATURE_FERN_00");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Environment/NATURE_FERN_00");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"NATURE_FERN_00");
			obj->GetTransform()->SetLocalPosition(Vec3(movex, 0, 0));
			movex += 5.f;
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(renderShader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}

			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"NATURE_FERN_04", L"Environment/NATURE_FERN_04");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Environment/NATURE_FERN_04");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"NATURE_FERN_04");
			obj->GetTransform()->SetLocalPosition(Vec3(movex, 0, 0));
			movex += 5.f;
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(renderShader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}

			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Vegetation_Shurb_01A", L"Environment/Vegetation_Shurb_01A");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Environment/Vegetation_Shurb_01A");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Vegetation_Shurb_01A");
			obj->GetTransform()->SetLocalPosition(Vec3(movex, 0, 0));
			movex += 5.f;
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(renderShader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}

			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Water_Grass_01", L"Environment/Water_Grass_01");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Environment/Water_Grass_01");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Water_Grass_01");
			obj->GetTransform()->SetLocalPosition(Vec3(movex, 0, 0));
			movex += 5.f;
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(renderShader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}

			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Water_Grass_03_1", L"Environment/Water_Grass_03_1");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Environment/Water_Grass_03_1");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Water_Grass_03_1");
			obj->GetTransform()->SetLocalPosition(Vec3(movex, 0, 0));
			movex += 5.f;
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(renderShader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}

			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Wetland_GrassPatch_2", L"Environment/Wetland_GrassPatch_2");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Environment/Wetland_GrassPatch_2");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Wetland_GrassPatch_2");
			obj->GetTransform()->SetLocalPosition(Vec3(movex, 0, 0));
			movex += 5.f;
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(renderShader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}

			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Wetland_GrassPatch_3", L"Environment/Wetland_GrassPatch_3");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Environment/Wetland_GrassPatch_3");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Wetland_GrassPatch_3");
			obj->GetTransform()->SetLocalPosition(Vec3(movex, 0, 0));
			movex += 5.f;
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(renderShader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}

			CURSCENE->Add(obj);
		}

		{
			shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"Woodland_GrassPatch_01", L"Environment/Woodland_GrassPatch_01");
			//m2->ReadModel(L"forest/forest");
			m2->ReadMaterial(L"Environment/Woodland_GrassPatch_01");
			auto obj = make_shared<GameObject>();
			obj->SetName(L"Woodland_GrassPatch_01");
			obj->GetTransform()->SetLocalPosition(Vec3(movex, 0, 0));
			movex += 5.f;
			obj->GetTransform()->SetLocalScale(Vec3(0.02f));
			obj->SetType(OBJECTTYPE::MAP);

			obj->AddComponent(make_shared<ModelRenderer>(renderShader));
			{
				obj->GetModelRenderer()->SetModel(m2);
				obj->GetModelRenderer()->SetPass(1);
			}

			CURSCENE->Add(obj);
		}

	}

	{
		// Light
		auto light = make_shared<GameObject>();
		light->AddComponent(make_shared<Light>());

		LightDesc lightDesc;
		lightDesc.ambient = Vec4(0.4f);
		lightDesc.diffuse = Vec4(1.f);
		lightDesc.specular = Vec4(0.1f);
		lightDesc.direction = Vec3(1.f, 1.f, 1.f);
		//light->GetLight()->SetLightDesc(lightDesc);
		light->GetTransform()->SetRotation(lightDesc.direction);
		light->GetTransform()->SetPosition(Vec3(0.f, 150.f, 0.f));
		static_pointer_cast<Light>(light->GetFixedComponent(ComponentType::Light))->SetLightDesc(lightDesc);
		CURSCENE->Add(light);
	}

	// Billboard
	//{
	//	shared_ptr<Shader> shader = make_shared<Shader>(L"28. BillboardDemo.fx");
	//	auto obj = make_shared<GameObject>();
	//	obj->GetTransform()->SetLocalPosition(Vec3(0.f));
	//	obj->AddComponent(make_shared<Billboard>());
	//	{
	//		// Material
	//		{
	//			shared_ptr<Material> material = make_shared<Material>();
	//			material->SetShader(shader);
	//			auto texture = RESOURCES->Load<Texture>(L"Grass", L"..\\Resources\\Textures\\grass.png");
	//			//auto texture = RESOURCES->Load<Texture>(L"Veigar", L"..\\Resources\\Textures\\veigar.jpg");
	//			material->SetDiffuseMap(texture);
	//			MaterialDesc& desc = material->GetMaterialDesc();
	//			desc.ambient = Vec4(1.f);
	//			desc.diffuse = Vec4(1.f);
	//			desc.specular = Vec4(1.f);
	//			RESOURCES->Add(L"Veigar", material);

	//			obj->GetBillboard()->SetMaterial(material);
	//		}
	//	}

	//	for (int32 i = 0; i < 1000; i++)
	//	{
	//		Vec2 scale = Vec2(1 + rand() % 3, 1 + rand() % 3);
	//		Vec2 position = Vec2(-100 + rand() % 200, -100 + rand() % 200);

	//		obj->GetBillboard()->Add(Vec3(position.x, scale.y * 0.5f, position.y), scale);
	//	}

	//	CURSCENE->Add(obj);
	//}

	//Particle
	/*{
		auto particleShader = make_shared<Shader>(L"ParticleSystem.fx");
		auto obj = make_shared<GameObject>();
		obj->GetTransform()->SetLocalPosition(Vec3(0.f, 5.f, 0.f));
		obj->AddComponent(make_shared<ParticleSystem>());
		shared_ptr<ParticleSystem> particleSystem = obj->GetFixedComponent<ParticleSystem>(ComponentType::ParticleSystem);
		particleSystem->SetEmitDirW(Vec3(0.f, 2.f, 0.f));
		shared_ptr<Material> material = make_shared<Material>();
		material->SetRenderQueue(RenderQueue::Transparent);
		material->SetShader(particleShader);
		auto texture = RESOURCES->Load<Texture>(L"Flare", L"..\\Resources\\Textures\\flare0.png");
		material->SetDiffuseMap(texture);
		material->SetRandomTex(RESOURCES->Get<Texture>(L"RandomTex"));
		particleSystem->SetMaterial(material);
		CURSCENE->Add(obj);
	}*/

	{
		// Animation
		shared_ptr<Model> m1 = make_shared<Model>();

		m1->ReadModel(L"Bianca2/Bianca");
		m1->ReadMaterial(L"Bianca2/Bianca");
		m1->ReadAnimation(L"Wait", L"Bianca2/Bianca_wait");
		m1->ReadAnimation(L"Run",L"Bianca2/Bianca_run");

		m1->ReadAnimation(L"Skill_1", L"Bianca2/Bianca_skill1");

		m1->ReadAnimation(L"Skill_3_1", L"Bianca2/Bianca_skill3-1");
		m1->ReadAnimation(L"Skill_3_2", L"Bianca2/Bianca_skill3-2");
		m1->ReadAnimation(L"Skill_3_3", L"Bianca2/Bianca_skill3-3");


		m1->ReadAnimation(L"Skill_4_1", L"Bianca2/Bianca_skill4");
		m1->ReadAnimation(L"Skill_4_2", L"Bianca2/Bianca_skill4-2");

		//m1->ReadAnimation(L"Run", L"Bianca2/Bianca_run");
		//m1->ReadAnimation(L"Skill_R_1",L"Bianca2/Bianca_skill4");
		//m1->ReadAnimation(L"Skill_R_2", L"Bianca2/Bianca_skill4-2");


		for (int32 i = 0; i < 1; i++)
		{

			auto obj = make_shared<GameObject>();
			obj->GetTransform()->SetPosition(Vec3(0.f, 0.f, 0.f));
			obj->GetTransform()->SetScale(Vec3(1.f));

			obj->AddComponent(make_shared<ModelAnimator>(renderShader));
			{
				obj->GetModelAnimator()->SetModel(m1);
				obj->GetModelAnimator()->SetPass(2);
			}
			obj->AddComponent(make_shared<AABBBoxCollider>());
			obj->AddComponent(make_shared<FogOfWar>());
			obj->AddComponent(make_shared<BiancaTest>());
			auto animator = obj->GetModelAnimator();
			// FSM 추가
			auto stateMachine = make_shared<AnimationStateMachine>();
			obj->AddComponent(stateMachine);

			/*obj->GetAnimationStateMachine()->RegisterState(AnimationStateType::Wait, make_shared<BiancaWaitState>());
			obj->GetAnimationStateMachine()->RegisterState(AnimationStateType::Run, make_shared<BiancaRunState>());
			obj->GetAnimationStateMachine()->RegisterState(AnimationStateType::Skill_1, make_shared<BiancaQSkillState>());
			obj->GetAnimationStateMachine()->RegisterState(AnimationStateType::Skill_3, make_shared<BiancaESkillState>());
			obj->GetAnimationStateMachine()->RegisterState(AnimationStateType::Skill_4, make_shared<BiancaRSkillState>());*/

			// Q 스킬 시퀀스 
			vector<wstring> skill1Anims = { L"Skill_1" };
			animator->CreateSequence(L"Skill_1_Sequence", skill1Anims, false);

			// R 스킬 시퀀스 (Skill_04_Ready -> Skill_04_Start -> Skill_04_Attack)
			vector<wstring> skill4Anims = { L"Skill_4_1", L"Skill_4_2" }; 
			animator->CreateSequence(L"Skill_4_Sequence", skill4Anims, false);

			CURSCENE->Add(obj);

			//camera->GetTransform()->SetParent(obj->GetTransform());
			//auto BiancaCam = make_shared<BiancaCamera>();
			//camera->AddComponent(BiancaCam);
			//BiancaCam->SetTarget(obj);
			//BiancaCam->SetOffset(Vec3(0.f, 12.f, -12.5f));
			//camera->GetTransform()->SetRotation(Vec3{ 45.f, 0.f, 0.f });

		}
	}


	//교회 Base
	//{
	//	// Model
	//	shared_ptr<Model> m2 = make_shared<Model>();
	//	m2->ReadModel(L"/Cemetary");
	//	m2->ReadMaterial(L"Cemetary/Cemetary");
	//	//m2->ReadModel(L"map2/map2");
	//	//m2->ReadMaterial(L"map2/map2");

	//	for (int32 i = 0; i < 1; i++)
	//	{
	//		auto obj = make_shared<GameObject>();
	//		obj->GetTransform()->SetPosition(Vec3(0, 0, 0));
	//		obj->GetTransform()->SetScale(Vec3(0.01f));
	//		obj->SetType(OBJECTTYPE::MAP);

	//		obj->AddComponent(make_shared<ModelRenderer>(renderShader));
	//		{
	//			obj->GetModelRenderer()->SetModel(m2);
	//			obj->GetModelRenderer()->SetPass(1);
	//		}

	//		CURSCENE->Add(obj);
	//	}
	//}
	//교회. 
	//맵 부모 오브젝트. 
	//{
	//	auto baseobj = make_shared<GameObject>();
	//	baseobj->GetTransform()->SetLocalPosition(Vec3(0, 0, 0));
	//	baseobj->GetTransform()->SetLocalScale(Vec3(1.f));
	//	baseobj->SetType(OBJECTTYPE::MAP);
	//	//교회 Base
	//	{
	//		shared_ptr<Model> m2 = RESOURCES->GetOrAddModel(L"forest", L"forest/forest");
	//		//m2->ReadModel(L"forest/forest");
	//		m2->ReadMaterial(L"forest/forest");
	//		auto obj = make_shared<GameObject>();
	//		obj->SetName(L"forest");
	//		obj->GetTransform()->SetLocalPosition(Vec3(0, 0, 0));
	//		obj->GetTransform()->SetLocalScale(Vec3(0.01f));
	//		obj->GetTransform()->SetParent(baseobj->GetTransform());
	//		obj->SetType(OBJECTTYPE::MAP);

	//		obj->AddComponent(make_shared<ModelRenderer>(renderShader));
	//		{
	//			obj->GetModelRenderer()->SetModel(m2);
	//			obj->GetModelRenderer()->SetPass(1);
	//		}

	//		CURSCENE->Add(obj);
	//	}

	//}

}

void BillboardDemo::Update()
{
}

void BillboardDemo::Render()
{

}

//버튼이라는 클래스는 OBB Collision을 두고. 
//Ray를 쏘는 방식으로...
//그런데, 우리는 그냥 WINAPI방식으로 한다. Collision방식은 부하가 있음. 
//그림자는 무조건 들어가는 게 좋음. 

//포폴을 만들 때 중요한 건 시간과 노력
//충돌, 레이캐스팅, 애니메이션, 매쉬 로드등과 같은 것. 

void moveScript::Update()
{
	Vec3 pos = GetTransform()->GetPosition();

	pos.x += DT * 1.5f;

	GetTransform()->SetPosition(pos);
}

void ForceScript::Start()
{
	GetRigidbody()->AddForce(Vec3(-8.f, 0.f, 0.f));
}

void ForceScript::Update()
{
	GetRigidbody()->AddForce(Vec3(0.f, 0.f, 0.f));
}


