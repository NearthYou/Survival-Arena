#pragma once

template<typename T>
class ConstantBuffer
{
public:
	ConstantBuffer() { }
	~ConstantBuffer()
	{
		m_constantBuffer.Reset();
	}
	

	ComPtr<ID3D11Buffer> GetComPtr() { return m_constantBuffer; }
	//템플릿 함수는 컴파일러가 컴파일 타임에 
	//선언부만 봐서는 타입을 알 수가 없다.
	//따라서 구현파일이 아닌, 선언 할 때 한 번에 정의 해줘야 한다. 
	void Create()
	{
		D3D11_BUFFER_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Usage = D3D11_USAGE_DYNAMIC; // CPU_Write + GPU_Read
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.ByteWidth = sizeof(T);
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HRESULT hr = DEVICE->CreateBuffer(&desc, nullptr, m_constantBuffer.GetAddressOf());
		CHECK(hr);
	}

	void CopyData(const T& data)
	{
		D3D11_MAPPED_SUBRESOURCE subResource;
		ZeroMemory(&subResource, sizeof(subResource));

		DC->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &subResource);
		::memcpy(subResource.pData, &data, sizeof(data));
		DC->Unmap(m_constantBuffer.Get(), 0);
	}

private:
	ComPtr<ID3D11Buffer> m_constantBuffer;
};
