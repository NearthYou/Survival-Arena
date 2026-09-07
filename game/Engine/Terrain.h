#pragma once
#include "Component.h"
class Terrain :
    public Component
{
	using Super = Component;

public:
	Terrain();
	~Terrain();

	void Create(int32 _sizeX, int32 _sizeZ, shared_ptr<Material> _material);

	int32 GetSizeX() { return m_sizeX; }
	int32 GetSizeZ() { return m_sizeZ; }

	bool Pick(int32 _screenX, int32 _screenY, Vec3& _pickPos, float& _distance);

private:
	shared_ptr<Mesh> m_mesh;
	int32 m_sizeX = 0;
	int32 m_sizeZ = 0;
};

