#pragma once
class Sky
{
public:
	Sky(const std::wstring& _cubemapFilename, const wstring& _shaderFileName);
	~Sky();

	 ComPtr<ID3D11ShaderResourceView> CubeMapSRV();

	 void Render(Camera* _camera);

private:
	shared_ptr<VertexBuffer> m_vb;
	shared_ptr<IndexBuffer> m_ib;

	shared_ptr<class Texture> m_texture;
	shared_ptr<class Material> m_material;
	uint32 m_indexCount;
};

