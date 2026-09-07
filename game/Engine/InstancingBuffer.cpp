#include "pch.h"
#include "InstancingBuffer.h"

InstancingBuffer::InstancingBuffer()
{
	CreateBuffer(MAX_MESH_INSTANCE);
}

InstancingBuffer::~InstancingBuffer()
{
	
}

void InstancingBuffer::ClearData()
{
	m_data.clear();
}

void InstancingBuffer::AddData(InstancingData& _data)
{
	m_data.push_back(_data);
}

void InstancingBuffer::PushData()
{
	const uint32 dataCount = GetCount();
	if (dataCount > m_maxCount) {
		CreateBuffer(dataCount);
	}

	//GPU에 넣어주기. 
	D3D11_MAPPED_SUBRESOURCE subResource;
	DC->Map(m_instanceBuffer->GetComPtr().Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &subResource);
	{
		::memcpy(subResource.pData, m_data.data(), sizeof(InstancingData) * dataCount);
	}
	DC->Unmap(m_instanceBuffer->GetComPtr().Get(), 0);

	m_instanceBuffer->PushData();
}

void InstancingBuffer::CreateBuffer(uint32 _maxCount)
{

	m_maxCount = _maxCount;
	m_instanceBuffer = make_shared<VertexBuffer>();

	vector<InstancingData> temp(_maxCount);

	m_instanceBuffer->Create(temp, 1, true);
}
