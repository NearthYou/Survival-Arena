#include "pch.h"
#include "SnowBillboard.h"
#include "Material.h"
#include "Camera.h"
#include "MathUtils.h"


SnowBillboard::SnowBillboard(Vec3 _start, Vec3 _extent, int32 _drawCount /*= 100*/)
	:Super(ComponentType::SnowBillboard)
{
	m_desc.m_origin = _start;
	m_desc.m_extent = _extent;
	m_desc.m_drawDistance = _extent.z * 2.0f;
	m_drawCount = _drawCount;

	const int32 vertexCount = _drawCount * 4;
	m_vertices.resize(vertexCount);

	for (int32 i = 0; i < m_drawCount * 4; i += 4)
	{
		Vec2 scale;
		scale.x = MathUtils::Random(0.02f, 0.08f);
		scale.y = MathUtils::Random(0.3f, 0.5f);

		Vec3 position;
		position.x = MathUtils::Random(-m_desc.m_extent.x, m_desc.m_extent.x);
		position.y = MathUtils::Random(-m_desc.m_extent.y, m_desc.m_extent.y);
		position.z = MathUtils::Random(-m_desc.m_extent.z, m_desc.m_extent.z);

		Vec2 random = MathUtils::RandomVec2(0.0f, 1.0f);

		m_vertices[i + 0].pos = position;
		m_vertices[i + 1].pos = position;
		m_vertices[i + 2].pos = position;
		m_vertices[i + 3].pos = position;
		
		m_vertices[i + 0].uv = Vec2(0, 1);
		m_vertices[i + 1].uv = Vec2(0, 0);
		m_vertices[i + 2].uv = Vec2(1, 1);
		m_vertices[i + 3].uv = Vec2(1, 0);
		
		m_vertices[i + 0].scale = scale;
		m_vertices[i + 1].scale = scale;
		m_vertices[i + 2].scale = scale;
		m_vertices[i + 3].scale = scale;
		
		m_vertices[i + 0].random = random;
		m_vertices[i + 1].random = random;
		m_vertices[i + 2].random = random;
		m_vertices[i + 3].random = random;
	}

	m_vertexBuffer = make_shared<VertexBuffer>();
	m_vertexBuffer->Create(m_vertices, 0);

	const int32 indexCount = m_drawCount * 6;
	m_indices.resize(indexCount);

	for (int32 i = 0; i < m_drawCount; i++)
	{
		m_indices[i * 6 + 0] = i * 4 + 0;
		m_indices[i * 6 + 1] = i * 4 + 1;
		m_indices[i * 6 + 2] = i * 4 + 2;
		m_indices[i * 6 + 3] = i * 4 + 2;
		m_indices[i * 6 + 4] = i * 4 + 1;
		m_indices[i * 6 + 5] = i * 4 + 3;
	}

	m_indexBuffer = make_shared<IndexBuffer>();
	m_indexBuffer->Create(m_indices);
}

SnowBillboard::~SnowBillboard()
{
	m_vertexBuffer.reset();
	m_indexBuffer.reset();
}

void SnowBillboard::SetMaterial(shared_ptr<Material> _material)
{
	Super::SetMaterial(_material);
	m_material->SetCastShadow(false);

}

void SnowBillboard::SetParticleScale(float _scale)
{
	for (auto& vertice : m_vertices) {
		vertice.scale *= _scale;
	}

	if (m_vertexBuffer) {
		m_vertexBuffer->Create(m_vertices, 0);
	}
}

void SnowBillboard::SetParticleScale(Vec2 _scale)
{
	for (auto& vertice : m_vertices) {
		vertice.scale = _scale;
	}

	if (m_vertexBuffer) {
		m_vertexBuffer->Create(m_vertices, 0);
	}
}

void SnowBillboard::SetColor(Vec4 _color)
{
	m_desc.m_color = _color;
}

void SnowBillboard::InnerRender(bool _isShadowTech)
{
	assert(!_isShadowTech);
	Super::InnerRender(_isShadowTech);


	//m_desc.m_origin = CURSCENE->GetMainCamera()->GetTransform()->GetPosition();
	m_desc.m_time = m_elapsedTime;
	m_elapsedTime += DT;

	auto shader = m_material->GetShader();

	// SnowData
	shader->PushSnowData(m_desc);

	// IA
	m_vertexBuffer->PushData();
	m_indexBuffer->PushData();

	//shader->DrawIndexed(0, m_pass, m_drawCount * 6);

	shader->DrawIndexed(GET_TECH(_isShadowTech), m_pass, m_drawCount * 6);
}
