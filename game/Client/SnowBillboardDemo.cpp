#include "pch.h"
#include "SnowBillboardDemo.h"
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
#include "SnowBillboard.h"


void SnowBillboardDemo::Init()
{
	m_shader = make_shared<Shader>(L"29. SnowBillboard.fx");

	// Camera
	{
		auto camera = make_shared<GameObject>();
		camera->GetTransform()->SetPosition(Vec3{ 0.f, 5.f, -5.f });
		camera->AddComponent(make_shared<Camera>());
		camera->GetCamera()->SetCullingMaskLayerOnOff(LAYER_UI, true);
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


	// Billboard
	{
		auto obj = make_shared<GameObject>();
		obj->GetTransform()->SetLocalPosition(Vec3(0.f));
		obj->AddComponent(make_shared<SnowBillboard>(Vec3(0, 0, 0), Vec3(200, 200, 200), 20000));
		{
			// Material
			{
				shared_ptr<Material> material = make_shared<Material>();
				material->SetShader(m_shader);
				auto texture = RESOURCES->Load<Texture>(L"Veigar", L"..\\Resources\\Textures\\grass.png");
				//auto texture = RESOURCES->Load<Texture>(L"Veigar", L"..\\Resources\\Textures\\veigar.jpg");
				material->SetDiffuseMap(texture);
				MaterialDesc& desc = material->GetMaterialDesc();
				desc.ambient = Vec4(1.f);
				desc.diffuse = Vec4(1.f);
				desc.specular = Vec4(1.f);
				RESOURCES->Add(L"Veigar", material);

				obj->GetSnowBillboard()->SetMaterial(material);
			}
		}

		CURSCENE->Add(obj);
	}

	
}

void SnowBillboardDemo::Update()
{
}

void SnowBillboardDemo::Render()
{

}
