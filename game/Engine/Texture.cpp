#include "pch.h"
#include "Texture.h"
#include <filesystem>

Texture::Texture() : Super(ResourceType::Texture)
{

}

Texture::~Texture()
{

}

void Texture::Load(const wstring& _path)
{


	
	wstring ext = filesystem::path(_path).extension();

	DirectX::TexMetadata md;
	//HRESULT hr = ::LoadFromWICFile(_path.c_str(), WIC_FLAGS_NONE, &md, m_img);

	HRESULT hr;
	if (ext == L".dds" || ext == L".DDS")
		hr = ::LoadFromDDSFile(_path.c_str(), DDS_FLAGS_NONE, &md, m_img);
	else if(ext == L".tga" || ext == L".TGA")
		hr = ::LoadFromTGAFile(_path.c_str(), &md, m_img);
	else //png, jpg, jpeg, bmp
		hr = ::LoadFromWICFile(_path.c_str(), WIC_FLAGS_NONE, &md, m_img);

	CHECK(hr);

	hr = ::CreateShaderResourceView(DEVICE.Get(), m_img.GetImages(), m_img.GetImageCount(), md, m_shaderResourceView.GetAddressOf());
	CHECK(hr);
	
	m_shaderResourceView->SetPrivateData(
		WKPDID_D3DDebugObjectName,
		sizeof("Texture::m_shaderResourceview") - 1,
		"Texture::m_shaderResourceview"
	);
	

	m_size.x = md.width;
	m_size.y = md.height;
}

ComPtr<ID3D11Texture2D> Texture::GetTexture2D()
{
	ComPtr<ID3D11Texture2D> texture;
	
	m_shaderResourceView->GetResource((ID3D11Resource**)texture.GetAddressOf());

	return texture;
}

