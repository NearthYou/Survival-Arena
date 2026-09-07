#pragma once

class VertexBuffer
{
public:
	VertexBuffer();
	~VertexBuffer();

	ComPtr<ID3D11Buffer> GetComPtr() { return _vertexBuffer; }
	uint32 GetStride() { return m_stride; }
	uint32 GetOffset() { return m_offset; }
	uint32 GetCount() { return m_count; }
	uint32 GetSlot() { return m_slot; }

	template<typename T>
	void Create(const vector<T>& _vertices, uint32 _slot = 0, bool _cpuWrite = false, bool _gpuWrite = false)
	{
		m_stride = sizeof(T);
		m_count = static_cast<uint32>(_vertices.size());

		m_slot = _slot;
		m_cpuWrite = _cpuWrite;
		m_gpuWrite = _gpuWrite;

		D3D11_BUFFER_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.ByteWidth = (uint32)(m_stride * m_count);

		if (_cpuWrite == false && _gpuWrite == false)
		{
			desc.Usage = D3D11_USAGE_IMMUTABLE; // CPU Read, GPU Read
		}
		else if (_cpuWrite == true && _gpuWrite == false)
		{
			desc.Usage = D3D11_USAGE_DYNAMIC; // CPU Write, GPU Read
			desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
		}
		else if (_cpuWrite == false && _gpuWrite == true) // CPU Read, GPU Write
		{
			desc.Usage = D3D11_USAGE_DEFAULT;
		}
		else
		{
			desc.Usage = D3D11_USAGE_STAGING;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		}


		D3D11_SUBRESOURCE_DATA data;
		ZeroMemory(&data, sizeof(data));
		data.pSysMem = _vertices.data();

		HRESULT hr = DEVICE->CreateBuffer(&desc, &data, _vertexBuffer.GetAddressOf());
		_vertexBuffer->SetPrivateData(
			WKPDID_D3DDebugObjectName,
			sizeof("VertexBuffer::_vertexBuffer") - 1,
			"VertexBuffer::_vertexBuffer"
		);
		CHECK(hr);
	}

	void PushData()
	{
		DC->IASetVertexBuffers(m_slot, 1, _vertexBuffer.GetAddressOf(), &m_stride, &m_offset);
	}


	template<typename T>
	void CreateStreamOut(const int _maxVertexSize) {

		m_stride = sizeof(T);
		D3D11_BUFFER_DESC vbd;
		vbd.Usage = D3D11_USAGE_DEFAULT;

		vbd.CPUAccessFlags = 0;
		vbd.MiscFlags = 0;
		vbd.StructureByteStride = 0;

		vbd.ByteWidth = sizeof(T) * _maxVertexSize;
		vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_STREAM_OUTPUT;

		HRESULT hr = DEVICE->CreateBuffer(&vbd, 0, _vertexBuffer.GetAddressOf());

		CHECK(hr);
	}


private:
	ComPtr<ID3D11Buffer> _vertexBuffer;

	uint32 m_stride = 0;
	uint32 m_offset = 0;
	uint32 m_count = 0;

	uint32 m_slot = 0;
	bool m_cpuWrite = false;
	bool m_gpuWrite = false;
};
