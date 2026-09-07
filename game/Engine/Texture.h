#pragma once
#include "ResourceBase.h"

class Texture : public ResourceBase
{
	using Super = ResourceBase;
public:
	Texture();
	~Texture();

	ComPtr<ID3D11ShaderResourceView> GetComPtr() { return m_shaderResourceView; }

	virtual void Load(const wstring& _path) override;

	ComPtr<ID3D11Texture2D> GetTexture2D();
	void SetSRV(ComPtr<ID3D11ShaderResourceView> _srv) { m_shaderResourceView = _srv; }

	Vec2 GetSize() { return m_size; }

	const DirectX::ScratchImage& GetInfo() { return m_img; }

private:
	ComPtr<ID3D11ShaderResourceView> m_shaderResourceView;
	Vec2 m_size = {0.f, 0.f};
	DirectX::ScratchImage m_img = {};
};

