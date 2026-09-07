#pragma once

class VertexBuffer;

struct InstancingData {
	Matrix m_world;
};

#define MAX_MESH_INSTANCE 250



class InstancingBuffer
{
public:
	InstancingBuffer();
	~InstancingBuffer();

public:
	void ClearData();
	void AddData(InstancingData& _data);
	void PushData();

public:
	uint32 GetCount() { return static_cast<uint32>(m_data.size()); }
	shared_ptr<VertexBuffer> GetBuffer() { return m_instanceBuffer; }


private:
	void CreateBuffer(uint32 _maxCount = MAX_MESH_INSTANCE);


private:
	uint64						m_instanceID = 0;
	shared_ptr<VertexBuffer>	m_instanceBuffer;
	uint32						m_maxCount;
	vector<InstancingData>		m_data;
};

