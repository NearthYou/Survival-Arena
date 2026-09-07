#include "pch.h"
#include "ParticleDemo.h"
#include "GameObject.h"
#include "GeometryHelper.h"
#include "Camera.h"
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
#include "TextureBuffer.h"
#include "Viewport.h"
#include "SphereCollider.h"
#include "Scene.h"
#include "AABBBoxCollider.h"
#include "OBBBoxCollider.h"
#include "Terrain.h"
#include "Button.h"
#include "Billboard.h"
#include "ParticleSystem.h"

void ParticleDemo::Init()
{
	{
		// Camera
		auto camera = make_shared<GameObject>();
		camera->GetTransform()->SetPosition(Vec3{ 0.f, 5.f, -5.f });
		camera->AddComponent(make_shared<Camera>());
		camera->GetCamera()->SetCullingMaskLayerOnOff(LAYER_UI, true);
		CURSCENE->Add(camera);
	}

	// Terrain
	{
		auto terrainShader = make_shared<Shader>(L"23. RenderDemo.fx");
		{
			shared_ptr<Material> material = make_shared<Material>();
			material->SetShader(terrainShader);
			auto texture = RESOURCES->Load<Texture>(L"TerrainGrass", L"..\\Resources\\Textures\\Terrain\\grass.jpg");
			material->SetDiffuseMap(texture);
			MaterialDesc& desc = material->GetMaterialDesc();
			desc.ambient = Vec4(1.f);
			desc.diffuse = Vec4(1.f);
			desc.specular = Vec4(1.f);
			RESOURCES->Add(L"TerrainGrass", material);
		}
		auto obj = make_shared<GameObject>();
		obj->GetTransform()->SetLocalPosition(Vec3(-100.f, 0.f, -100.f));
		obj->AddComponent(make_shared<Terrain>());
		obj->GetTerrain()->Create(200, 200, RESOURCES->Get<Material>(L"TerrainGrass"));
		CURSCENE->Add(obj);
	}
	//Particle System
	
	{
		auto particleShader = make_shared<Shader>(L"ParticleSystem.fx");
		auto obj = make_shared<GameObject>();
		obj->GetTransform()->SetLocalPosition(Vec3(0.f, 5.f, 0.f));
		obj->AddComponent(make_shared<ParticleSystem>());
		shared_ptr<ParticleSystem> particleSystem = obj->GetFixedComponent<ParticleSystem>(ComponentType::ParticleSystem);
		shared_ptr<Material> material = make_shared<Material>();
		material->SetShader(particleShader);
		//auto texture = RESOURCES->Load<Texture>(L"TerrainGrass", L"..\\Resources\\Textures\\Terrain\\grass.jpg");
		material->SetDiffuseMap(RESOURCES->Get<Texture>(L"TerrainGrass"));
		material->SetRandomTex(RESOURCES->Get<Texture>(L"RandomTex"));
		particleSystem->SetMaterial(material);
		CURSCENE->Add(obj);
	}
	
}

void ParticleDemo::Update()
{
	/*
	if (INPUT->GetButtonDown(KEY_TYPE::LBUTTON)) {
		int32 mouseX = INPUT->GetMousePos().x;
		int32 mouseY = INPUT->GetMousePos().y;

		// Picking
		auto pickObj = CURSCENE->Pick(mouseX, mouseY);
		if (pickObj)
		{
			//CURSCENE->Remove(pickObj);
		}
	}
	*/
}

void ParticleDemo::Render()
{
	DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

//버튼이라는 클래스는 OBB Collision을 두고. 
//Ray를 쏘는 방식으로...
//그런데, 우리는 그냥 WINAPI방식으로 한다. Collision방식은 부하가 있음. 
//그림자는 무조건 들어가는 게 좋음. 

//포폴을 만들 때 중요한 건 시간과 노력
//충돌, 레이캐스팅, 애니메이션, 매쉬 로드등과 같은 것. 
