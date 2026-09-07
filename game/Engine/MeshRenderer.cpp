#include "pch.h"
#include "MeshRenderer.h"
#include "Camera.h"
#include "Game.h"
#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include "Light.h"



MeshRenderer::MeshRenderer() : Super(ComponentType::MeshRenderer)
{

}

MeshRenderer::~MeshRenderer()
{

}

/*
void MeshRenderer::Update()
{
	if (m_mesh == nullptr || m_texture == nullptr || m_shader == nullptr)
		return;


	m_shader->GetSRV("Texture0")->SetResource(m_texture->GetComPtr().Get());

	auto world = GetTransform()->GetWorldMatrix();
	RENDER->PushTransformData(TransformDesc{ world });
	//m_shader->GetConstantBuffer()

	uint32 stride = m_mesh->GetVertexBuffer()->GetStride();
	uint32 offset = m_mesh->GetVertexBuffer()->GetOffset();

	DC->IASetVertexBuffers(0, 1, m_mesh->GetVertexBuffer()->GetComPtr().GetAddressOf(), &stride, &offset);
	DC->IASetIndexBuffer(m_mesh->GetIndexBuffer()->GetComPtr().Get(), DXGI_FORMAT_R32_UINT, 0);

	m_shader->DrawIndexed(0, 0, m_mesh->GetIndexBuffer()->GetCount(), 0, 0);
}
*/
/*
void MeshRenderer::Update()
{
	if (m_mesh == nullptr || m_material == nullptr)
		return;

	auto shader = m_material->GetShader();

	if (shader == nullptr)
		return;

	m_material->Update();

	auto world = GetTransform()->GetWorldMatrix();
	RENDER->PushTransformData(TransformDesc{ world });
	//m_shader->GetConstantBuffer()

	uint32 stride = m_mesh->GetVertexBuffer()->GetStride();
	uint32 offset = m_mesh->GetVertexBuffer()->GetOffset();

	DC->IASetVertexBuffers(0, 1, m_mesh->GetVertexBuffer()->GetComPtr().GetAddressOf(), &stride, &offset);
	DC->IASetIndexBuffer(m_mesh->GetIndexBuffer()->GetComPtr().Get(), DXGI_FORMAT_R32_UINT, 0);

	shader->DrawIndexed(0, 0, m_mesh->GetIndexBuffer()->GetCount(), 0, 0);
}*/

void MeshRenderer::RenderInstancing(shared_ptr<class InstancingBuffer>& _buffer, bool _isShadowTech)
{
	if (!m_isActive) return;  // 비활성화되면 렌더링 안 함

	if (Super::Render(_isShadowTech) == false)
		return;


	if (m_mesh == nullptr)
		return;

	m_mesh->GetVertexBuffer()->PushData();
	m_mesh->GetIndexBuffer()->PushData();

	//Instancing의 PushData는 수많은 애들의 world matrix
	_buffer->PushData();

	m_material->GetShader()->DrawIndexedInstanced(GET_TECH(_isShadowTech), m_pass, m_mesh->GetIndexBuffer()->GetCount(), _buffer->GetCount());
	//shader->DrawIndexedInstanced(0, m_pass, m_mesh->GetIndexBuffer()->GetCount(), _buffer->GetCount());
}

void MeshRenderer::RenderInstancingDeferred(shared_ptr<class InstancingBuffer>& _buffer, bool _isShadowTech)
{
	// 기존 검증 로직 활용 (그림자 기법은 false로)
	if (Super::Render(false) == false)
		return;

	if (m_mesh == nullptr)
		return;

	// G-Buffer용 셰이더 사용 (포워드 셰이더 대신)
	auto geometryShader = m_material->GetShader();
	if (geometryShader == nullptr)
		return;

	// 기존 InnerRender 로직과 유사하지만 G-Buffer용 셰이더 사용
	DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	geometryShader->PushGlobalData(Camera::s_MatView, Camera::s_MatProjection);
	geometryShader->PushTransformData(TransformDesc{ GetTransform()->GetWorldMatrix() });

	m_mesh->GetVertexBuffer()->PushData();
	m_mesh->GetIndexBuffer()->PushData();
	_buffer->PushData();
	//geometryShader->SetTechnique(L"GBufferTech");
	geometryShader->DrawIndexedInstancedCurTech(m_pass,
		m_mesh->GetIndexBuffer()->GetCount(), _buffer->GetCount());
}

InstanceID MeshRenderer::GetInstanceID()
{
	//포인터를 통한 ID발급.
	return make_pair((uint64)m_mesh.get(), (uint64)m_material.get());
}
