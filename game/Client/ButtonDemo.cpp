#include "pch.h"
#include "ButtonDemo.h"
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
#include "TextureBuffer.h"
#include "Viewport.h"
#include "SphereCollider.h"
#include "Scene.h"
#include "AABBBoxCollider.h"
#include "OBBBoxCollider.h"
#include "Terrain.h"
#include "Button.h"
void ButtonDemo::Init()
{

	m_shader = make_shared<Shader>(L"23. RenderDemo.fx");

	// Camera
	{
		auto camera = make_shared<GameObject>();
		camera->GetTransform()->SetPosition(Vec3{ 0.f, 0.f, -5.f });
		camera->AddComponent(make_shared<Camera>());
		camera->GetCamera()->SetProjectionType(ProjectionType::Perspective);
		camera->GetCamera()->SetCullingMaskLayerOnOff(LAYER_UI, true);
		CURSCENE->Add(camera);
	}



	// UI Camera
	{
		auto camera = make_shared<GameObject>();
		camera->GetTransform()->SetPosition(Vec3{ 0.f, 0.f, -5.f });
		camera->AddComponent(make_shared<Camera>());
		camera->GetCamera()->SetProjectionType(ProjectionType::Orthographic);
		camera->GetCamera()->SetNear(1.f);
		camera->GetCamera()->SetFar(100.f);

		camera->GetCamera()->SetCullingMaskAll();
		//UI만 컬링하지 않겠다 -> UI만 보여준다. 
		camera->GetCamera()->SetCullingMaskLayerOnOff(LAYER_UI, false);
		CURSCENE->Add(camera);
	}
	// Light
	{
		auto light = make_shared<GameObject>();
		light->AddComponent(make_shared<Light>());
		LightDesc lightDesc;
		lightDesc.ambient = Vec4(0.4f);
		lightDesc.diffuse = Vec4(1.f);
		lightDesc.specular = Vec4(0.1f);
		lightDesc.direction = Vec3(1.f, 0.f, 1.f);
		light->GetLight()->SetLightDesc(lightDesc);
		CURSCENE->Add(light);
	}


	// Mesh
	// Material
	{
		shared_ptr<Material> material = make_shared<Material>();
		material->SetShader(m_shader);
		auto texture = RESOURCES->Load<Texture>(L"Veigar", L"..\\Resources\\Textures\\veigar.jpg");

		material->SetDiffuseMap(texture);
		MaterialDesc& desc = material->GetMaterialDesc();
		desc.ambient = Vec4(1.f);
		desc.diffuse = Vec4(1.f);
		desc.specular = Vec4(1.f);
		RESOURCES->Add(L"Veigar", material);
	}

	//Terrain
	/*
	{
		auto obj = make_shared<GameObject>();
		obj->AddComponent(make_shared<Terrain>());

		obj->GetTerrain()->Create(10, 10, RESOURCES->Get<Material>(L"Veigar"));
		obj->GetOrAddTransform()->SetLocalPosition(Vec3(-2.f, -2.f, -2.f));

		CURSCENE->Add(obj);
	}
	*/

	//Button
	{
		auto obj = make_shared<GameObject>();
		obj->AddComponent(make_shared<Button>());
		//obj->GetButton()->Create(Vec2(100, 100), Vec2(100, 100), RESOURCES->Get<Material>(L"Veigar"));
	
		obj->GetButton()->AddOnClickedEvent(ButtonDemo::Test);
		CURSCENE->Add(obj);
	}
	//Mesh
	{
		auto obj = make_shared<GameObject>();
		obj->GetTransform()->SetLocalPosition(Vec3(0.1f));
		obj->GetTransform()->SetScale(Vec3(2.f));
		obj->AddComponent(make_shared<MeshRenderer>());
		{
			obj->GetMeshRenderer()->SetMaterial(RESOURCES->Get<Material>(L"Veigar"));
		}
		{
			auto mesh = RESOURCES->Get<Mesh>(L"Sphere");
			obj->GetMeshRenderer()->SetMesh(mesh);
			obj->GetMeshRenderer()->SetPass(0);
		}
		CURSCENE->Add(obj);
	}
	/*
	for (int32 i = 0; i < 1; i++)
	{
		auto obj = make_shared<GameObject>();
		obj->GetOrAddTransform()->SetLocalPosition(Vec3(2.f, 0, 0));
		obj->AddComponent(make_shared<MeshRenderer>());
		{
			obj->GetMeshRenderer()->SetMaterial(RESOURCES->Get<Material>(L"Veigar"));
		}
		{
			auto mesh = RESOURCES->Get<Mesh>(L"Cube");
			obj->GetMeshRenderer()->SetMesh(mesh);
			obj->GetMeshRenderer()->SetPass(0);
		}
		{
			auto collider = make_shared<OBBBoxCollider>();
			collider->GetBoundingBox().Extents = Vec3(0.5f);
			collider->GetBoundingBox().Orientation = Quaternion::CreateFromYawPitchRoll(45, 0, 0);
			obj->AddComponent(collider);
		}
		{
			obj->AddComponent(make_shared<MoveScript>());
		}

		CURSCENE->Add(obj);
	}
	*/
	//RENDER->Init(_shader);

}

void ButtonDemo::Update()
{
}

void ButtonDemo::Render()
{

}

//버튼이라는 클래스는 OBB Collision을 두고. 
//Ray를 쏘는 방식으로...
//그런데, 우리는 그냥 WINAPI방식으로 한다. Collision방식은 부하가 있음. 
//그림자는 무조건 들어가는 게 좋음. 

//포폴을 만들 때 중요한 건 시간과 노력
//충돌, 레이캐스팅, 애니메이션, 매쉬 로드등과 같은 것. 