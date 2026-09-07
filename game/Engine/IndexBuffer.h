#pragma once

class IndexBuffer
{
public:
	IndexBuffer();
	~IndexBuffer();

	ComPtr<ID3D11Buffer> GetComPtr() { return _indexBuffer; }
	uint32 GetStride() { return m_stride; }
	uint32 GetOffset() { return m_offset; }
	uint32 GetCount() { return m_count; }

	void Create(const vector<uint32>& _indices);
	void PushData()
	{
		DC->IASetIndexBuffer(_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	}

private:
	ComPtr<ID3D11Buffer> _indexBuffer;

	uint32 m_stride = 0;
	uint32 m_offset = 0;
	uint32 m_count = 0;
};

