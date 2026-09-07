#include "pch.h"
#include "ModelMesh.h"

void ModelMesh::CreateBuffers()
{
	m_vertexBuffer = make_shared<VertexBuffer>();
	m_vertexBuffer->Create(m_geometry->GetVertices());


	m_indexBuffer = make_shared<IndexBuffer>();
	m_indexBuffer->Create(m_geometry->GetIndices());
}
