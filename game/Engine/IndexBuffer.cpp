#include "pch.h"
#include "IndexBuffer.h"

IndexBuffer::IndexBuffer()
{

}

IndexBuffer::~IndexBuffer()
{
	
}

void IndexBuffer::Create(const vector<uint32>& _indices)
{
	//stride란 uint32가 점유하고 있는 바이트(4바이트)
	m_stride = sizeof(uint32);
	m_count = static_cast<uint32>(_indices.size());

	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	desc.ByteWidth = (uint32)(m_stride * m_count);

	D3D11_SUBRESOURCE_DATA data;
	ZeroMemory(&data, sizeof(data));
	data.pSysMem = _indices.data();

	HRESULT hr = DEVICE->CreateBuffer(&desc, &data, _indexBuffer.GetAddressOf());

	CHECK(hr);
}
