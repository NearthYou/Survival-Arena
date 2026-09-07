#pragma once
class Viewport
{
public:
	Viewport();
	Viewport(float _width, float _height, float _x = 0, float _y = 0, float _minDepth = 0, float _maxDepth = 1);

	~Viewport();

	void RSSetViewport();
	void Set(float _width, float _height, float _x = 0, float _y = 0, float _minDepth = 0, float _maxDepth = 1);



	float GetWidth() { return m_vp.Width; }
	float GetHeight() { return m_vp.Height; }


	//2d에서 3d(world)로 가기. 
	Vec3 Project(const Vec3& pos, const Matrix& W, const Matrix& V, const Matrix& P);
	Vec3 UnProject(const Vec3& pos, const Matrix& W, const Matrix& V, const Matrix& P);
private:
	D3D11_VIEWPORT m_vp;
};

