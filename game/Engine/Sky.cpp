#include "pch.h"
#include "Sky.h"
#include "Camera.h"
#include "Material.h"

Sky::Sky(const std::wstring& _cubemapFilename, const wstring& _shaderFileName)
{
	//_cubeMapSRV = Utils::LoadTexture(device, cubemapFilename);
	m_texture = make_shared<Texture>();
	m_texture->Load(_cubemapFilename);

	shared_ptr<Mesh> sphereMesh = RESOURCES->Get<Mesh>(L"Sphere");
	auto sphereGeometry = sphereMesh->GetGeometry();

	const vector<VertexTextureNormalTangentData>& geoVerteices = sphereGeometry->GetVertices();

	std::vector<Vec3> vertices(geoVerteices.size());
	for (size_t i = 0; i < geoVerteices.size(); ++i)
	{
		vertices[i] = geoVerteices[i].position;
	}

	m_vb = make_shared<VertexBuffer>();
	m_vb->Create(vertices, 0, false, true);

	const vector<uint32>& geoindices = sphereGeometry->GetIndices();
	m_indexCount = geoindices.size();

	m_ib = make_shared<IndexBuffer>();
	m_ib->Create(geoindices);

	m_material = make_shared<Material>();
	m_material->SetShader(make_shared<Shader>(_shaderFileName));
	m_material->SetCubeMap(m_texture);
}

Sky::~Sky()
{
	
}

ComPtr<ID3D11ShaderResourceView> Sky::CubeMapSRV()
{
	return m_texture->GetComPtr();
}

void Sky::Render(Camera* _camera)
{
	Vec3 eyePos = _camera->GetTransform()->GetPosition();
	Matrix world = Matrix::CreateTranslation(eyePos);

	Matrix v = _camera->GetViewMatrix();
	Matrix p = _camera->GetProjectionMatrix();
	Matrix wvp = world * v * p;

	shared_ptr<Shader> shader = m_material->GetShader();
	shader->PushTransformData(TransformDesc{ wvp });
	shader->PushGlobalData(Camera::s_MatView, Camera::s_MatProjection);
	m_material->Update();

	m_vb->PushData();
	m_ib->PushData();
	DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	shader->DrawIndexed(0, 0, m_indexCount, 0, 0);
}