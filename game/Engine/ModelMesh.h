#pragma once

struct ModelBone {
	wstring m_name;
	int32 m_index;
	int32 m_parentIndex;
	shared_ptr<ModelBone> m_parent; //Cache

	Matrix m_transform;
	vector<shared_ptr<ModelBone>> children; // Cache;

	Matrix m_offsetMatrix;
};

struct ModelMesh
{
	void CreateBuffers();

	wstring m_name;

	// Mesh
	shared_ptr<Geometry<ModelVertexType>> m_geometry = make_shared<Geometry<ModelVertexType>>();
	shared_ptr<VertexBuffer> m_vertexBuffer;
	shared_ptr<IndexBuffer> m_indexBuffer;

	// Material
	wstring m_materialName = L"";
	shared_ptr<Material> m_material; // Cache

	// Bones
	int32 m_boneIndex;
	shared_ptr<ModelBone> m_bone; // Cache;

};

