#pragma once
#include "ResourceBase.h"
#include "Geometry.h"

class Mesh : public ResourceBase
{
    using Super = ResourceBase;

public:
    Mesh();
    virtual ~Mesh();

	void CreateQuad();
	void CreateCube();
	void CreateGrid(int32 sizeX, int32 sizeZ);
	void CreateSphere();
	void CreateCone();

	shared_ptr<VertexBuffer> GetVertexBuffer() { return m_vertexBuffer; }
	shared_ptr<IndexBuffer> GetIndexBuffer() { return m_indexBuffer; }
	shared_ptr<Geometry<VertexTextureNormalTangentData>> GetGeometry() { return m_geometry; }

private:
	void CreateBuffers();



private:
	// Mesh
	shared_ptr<Geometry<VertexTextureNormalTangentData>> m_geometry;
	shared_ptr<VertexBuffer> m_vertexBuffer;
	shared_ptr<IndexBuffer> m_indexBuffer;
};

