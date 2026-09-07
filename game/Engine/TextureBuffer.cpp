#include "pch.h"
#include "TextureBuffer.h"

TextureBuffer::TextureBuffer(ComPtr<ID3D11Texture2D> _src)
{
	//소스 이미지를 복사해서 Input으로 별도 관리. 
	CreateInput(_src);
	CreateBuffer();
}

TextureBuffer::~TextureBuffer()
{
	
}

void TextureBuffer::CreateBuffer()
{
	CreateSRV();
	CreateOutput();
	CreateUAV();
	CreateResult();
}

void TextureBuffer::CreateInput(ComPtr<ID3D11Texture2D> _src)
{
	D3D11_TEXTURE2D_DESC srcDesc;
	_src->GetDesc(&srcDesc);

	m_width = srcDesc.Width;
	m_height = srcDesc.Height;
	m_arraySize = srcDesc.ArraySize;
	m_format = srcDesc.Format;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = m_width;
	desc.Height = m_height;
	desc.ArraySize = m_arraySize;
	desc.Format = m_format;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;

	CHECK(DEVICE->CreateTexture2D(&desc, NULL, m_input.GetAddressOf()));

	DC->CopyResource(m_input.Get(), _src.Get());
}

void TextureBuffer::CreateSRV()
{
	D3D11_TEXTURE2D_DESC desc;
	m_input->GetDesc(&desc);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.ArraySize = m_arraySize;

	CHECK(DEVICE->CreateShaderResourceView(m_input.Get(), &srvDesc, m_srv.GetAddressOf()));
}

void TextureBuffer::CreateOutput()
{
	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
	desc.Width = m_width;
	desc.Height = m_height;
	desc.ArraySize = m_arraySize;
	desc.Format = m_format;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;

	CHECK(DEVICE->CreateTexture2D(&desc, nullptr, m_output.GetAddressOf()));
}

void TextureBuffer::CreateUAV()
{
	D3D11_TEXTURE2D_DESC desc;
	m_output->GetDesc(&desc);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Texture2DArray.ArraySize = m_arraySize;


	
	CHECK(DEVICE->CreateUnorderedAccessView(m_output.Get(), &uavDesc, m_uav.GetAddressOf()));
}

//결과는 ShaderResourceView를 쓰면 되니까. Output은 SRV로 준다. 
void TextureBuffer::CreateResult()
{
	D3D11_TEXTURE2D_DESC desc;
	m_output->GetDesc(&desc);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.ArraySize = m_arraySize;

	CHECK(DEVICE->CreateShaderResourceView(m_output.Get(), &srvDesc, m_outputSRV.GetAddressOf()));
}
