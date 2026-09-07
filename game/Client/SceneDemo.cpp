#include "pch.h"
#include "SceneDemo.h"
#include "GeometryHelper.h"
#include "Camera.h"
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

void SceneDemo::Init()
{
	RESOURCES->Init();
	m_shader = make_shared<Shader>(L"23. RenderDemo.fx");

	// Camera
	{
		auto camera = make_shared<GameObject>();
		camera->GetTransform()->SetPosition(Vec3{ 0.f, 0.f, -5.f });
		camera->AddComponent(make_shared<Camera>());

		CURSCENE->Add(camera);
	}

	//Light
	{
		auto light = make_shared<GameObject>();
		light->AddComponent(make_shared<Light>());

		LightDesc lightDesc;
		lightDesc.ambient = Vec4(0.4f);
		lightDesc.diffuse = Vec4(1.f);
		lightDesc.specular = Vec4(0.f);
		lightDesc.direction = Vec3(1.f, 0.f, 1.f);

		light->GetLight()->SetLightDesc(lightDesc);
		CURSCENE->Add(light);
	}

	//Animation
	shared_ptr<class Model> m1 = make_shared<Model>();
	m1->ReadModel(L"Kachujin/Kachujin");
	/*m1->ReadMaterial(L"Kachujin/Kachujin");
	m1->ReadAnimation(L"Kachujin/Idle");
	m1->ReadAnimation(L"Kachujin/Run");
	m1->ReadAnimation(L"Kachujin/Slash");*/

	

	for (int32 idx = 0; idx < 500; ++idx) {
		auto obj = make_shared<GameObject>();
		obj->GetTransform()->SetPosition(Vec3(rand() % 100, 0, rand() % 100));
		obj->GetTransform()->SetScale(Vec3(0.01f));
		obj->AddComponent(make_shared<ModelAnimator>(m_shader));
		{
			obj->GetModelAnimator()->SetModel(m1);
			obj->GetModelAnimator()->SetPass(2);
			//obj->GetModelRenderer()->SetModel(m1);
		}
		CURSCENE->Add(obj);
	}

	//Model
	shared_ptr<class Model> m2 = make_shared<Model>();
	m2->ReadModel(L"Tower/Tower");
	m2->ReadMaterial(L"Tower/Tower");


	for (int32 idx = 0; idx < 500; ++idx) {
		auto obj = make_shared<GameObject>();
		obj->GetTransform()->SetPosition(Vec3(rand() % 100, 0, rand() % 100));
		obj->GetTransform()->SetScale(Vec3(0.01f));
		obj->AddComponent(make_shared<ModelRenderer>(m_shader));
		{
			obj->GetModelRenderer()->SetModel(m2);
			obj->GetModelRenderer()->SetPass(1);
		}
		CURSCENE->Add(obj);
	}


	//CreateTower();
	//RENDER->init(m_shader);
}

void SceneDemo::Update()
{
}

void SceneDemo::Render()
{
}
