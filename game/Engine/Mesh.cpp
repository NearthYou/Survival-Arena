#include "pch.h"
#include "Mesh.h"
#include "GeometryHelper.h"

Mesh::Mesh()  : Super(ResourceType::Mesh)
{

}

Mesh::~Mesh()
{
	
}

void Mesh::CreateQuad()
{
	m_geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
	GeometryHelper::CreateQuad(m_geometry);
	CreateBuffers();
}

void Mesh::CreateCube()
{
	m_geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
	GeometryHelper::CreateCube(m_geometry);
	CreateBuffers();
}

void Mesh::CreateGrid(int32 sizeX, int32 sizeZ)
{
	m_geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
	GeometryHelper::CreateGrid(m_geometry, sizeX, sizeZ);
	CreateBuffers();
}

void Mesh::CreateSphere()
{
	m_geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
	GeometryHelper::CreateSphere(m_geometry);
	CreateBuffers();
}

void Mesh::CreateCone()
{
	m_geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
	GeometryHelper::CreateCone(m_geometry);
	CreateBuffers();
}

void Mesh::CreateBuffers()
{
	m_vertexBuffer = make_shared<VertexBuffer>();
	m_vertexBuffer->Create(m_geometry->GetVertices());
	m_indexBuffer = make_shared<IndexBuffer>();
	m_indexBuffer->Create(m_geometry->GetIndices());
}
