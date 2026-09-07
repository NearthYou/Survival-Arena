#pragma once
#include "Component.h"

class Camera;

class Light : public Component
{

public:
	Light();
	virtual ~Light();

	virtual void Update();


public:
	LightDesc& GetLightDesc() { return m_desc; }

	void SetLightDesc(LightDesc& _desc) { m_desc = _desc; }
	void SetAmbient(const Color& _color) { m_desc.ambient = _color; }
	void SetDiffuse(const Color& _color) { m_desc.diffuse = _color; }
	void SetSpecular(const Color& _color) { m_desc.specular = _color; }
	void SetEmissive(const Color& _color) { m_desc.emissive = _color; }
	void SetLightDirection(Vec3 _direction) { m_desc.direction = _direction; }

public:
	void SetVPMatrix(Camera* _camera, float _backDist, Matrix _matProjection);

public:
	static Matrix s_MatView;
	static Matrix s_MatProjection;
	static Matrix s_ShadowTransform;

private:
	LightDesc m_desc;
};

