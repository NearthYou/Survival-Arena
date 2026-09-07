#include "pch.h"
#include "Terrain.h"
#include "MeshRenderer.h"
#include "Camera.h"

Terrain::Terrain() : Super(ComponentType::Terrain)
{

}

Terrain::~Terrain()
{

}

void Terrain::Create(int32 _sizeX, int32 _sizeZ, shared_ptr<Material> _material)
{
	m_sizeX = _sizeX;
	m_sizeZ = _sizeZ;

	auto go = m_gameObject.lock();

	//go->GetOrAddTransform();
	go->GetTransform();


	if (go->GetMeshRenderer() == nullptr)
		go->AddComponent(make_shared<MeshRenderer>());

	m_mesh = make_shared<Mesh>();
	m_mesh->CreateGrid(_sizeX, _sizeZ);

	go->GetMeshRenderer()->SetMesh(m_mesh);
	go->GetMeshRenderer()->SetPass(0);
	go->GetMeshRenderer()->SetMaterial(_material);
}

bool Terrain::Pick(int32 screenX, int32 screenY, Vec3& pickPos, float& distance)
{
	Matrix W = GetTransform()->GetWorldMatrix();
	Matrix V = Camera::s_MatView;
	Matrix P = Camera::s_MatProjection;

	Viewport& vp = GRAPHICS->GetViewport();

	Vec3 n = vp.UnProject(Vec3(screenX, screenY, 0), W, V, P);
	Vec3 f = vp.UnProject(Vec3(screenX, screenY, 1), W, V, P);

	Vec3 start = n;
	Vec3 direction = f - n;
	direction.Normalize();

	Ray ray = Ray(start, direction);

	const auto& vertices = m_mesh->GetGeometry()->GetVertices();

	for (int32 z = 0; z < m_sizeZ; z++)
	{
		for (int32 x = 0; x < m_sizeX; x++)
		{
			uint32 index[4];
			index[0] = (m_sizeX + 1) * z + x;
			index[1] = (m_sizeX + 1) * z + x + 1;
			index[2] = (m_sizeX + 1) * (z + 1) + x;
			index[3] = (m_sizeX + 1) * (z + 1) + x + 1;

			Vec3 p[4];
			for (int32 i = 0; i < 4; i++)
				p[i] = vertices[index[i]].position;

			//  [2]
			//   |	\
			//  [0] - [1]
			if (ray.Intersects(p[0], p[1], p[2], OUT distance))
			{
				pickPos = ray.position + ray.direction * distance;
				return true;
			}

			//  [2] - [3]
			//   	\  |
			//		  [1]
			if (ray.Intersects(p[3], p[1], p[2], OUT distance))
			{
				pickPos = ray.position + ray.direction * distance;
				return true;
			}
		}
	}

	return false;
}
