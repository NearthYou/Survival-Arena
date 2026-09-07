#include "pch.h"
#include "ViewportDemo.h"
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

void ViewportDemo::Init()
{

	m_shader = make_shared<Shader>(L"23. RenderDemo.fx");

	// Camera
	{
		auto camera = make_shared<GameObject>();
		camera->GetTransform()->SetPosition(Vec3{ 0.f, 0.f, -5.f });
		camera->AddComponent(make_shared<Camera>());
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

	for (int32 i = 0; i < 1; i++)
	{
		auto obj = make_shared<GameObject>();
		obj->GetTransform()->SetLocalPosition(Vec3(0,0,0));
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

	//RENDER->Init(_shader);

}

void ViewportDemo::Update()
{
	static float width = 1366.f;
	static float height = 768.f;
	static float x = 0.f;
	static float y = 0.f;

	ImGui::InputFloat("Width", &width, 10.f);
	ImGui::InputFloat("Height", &height, 10.f);
	ImGui::InputFloat("X", &x, 10.f);
	ImGui::InputFloat("Y", &y, 10.f);

	GRAPHICS->SetViewport(width, height, x, y);


	static Vec3 pos = Vec3(2, 0, 0);
	ImGui::InputFloat3("Pos", (float*)&pos);

	Viewport& vp = GRAPHICS->GetViewport();
	Vec3 pos2D = vp.Project(pos, Matrix::Identity, Camera::s_MatView, Camera::s_MatProjection);

	ImGui::InputFloat3("Pos2D", (float*)&pos2D);

	{
		Vec3 temp = vp.UnProject(pos2D, Matrix::Identity, Camera::s_MatView, Camera::s_MatProjection);
		ImGui::InputFloat3("Recalc", (float*)&temp);
	}
}

void ViewportDemo::Render()
{

}

