#include "pch.h"
#include "StructuredBuffer.h"
StructuredBuffer::StructuredBuffer(void* _inputData, uint32 _inputStride, uint32 _inputCount, uint32 _outputStride, uint32 _outputCount)
	: m_inputData(_inputData), m_inputStride(_inputStride), m_inputCount(_inputCount), m_outputStride(_outputStride), m_outputCount(_outputCount)
{
	if (_outputStride == 0 || _outputCount == 0)
	{
		m_outputStride = _inputStride;
		m_outputCount = _inputCount;
	}

	CreateBuffer();
}

StructuredBuffer::~StructuredBuffer()
{
	
}

void StructuredBuffer::CreateBuffer()
{
	CreateInput();
	CreateSRV();
	CreateOutput();
	CreateUAV();
	CreateResult();
}

void StructuredBuffer::CreateInput()
{
	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.ByteWidth = GetInputByteWidth();
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = m_inputStride;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA subResource = { 0 };
	subResource.pSysMem = m_inputData;

	if (m_inputData != nullptr)
		CHECK(DEVICE->CreateBuffer(&desc, &subResource, m_input.GetAddressOf()));
	else
		CHECK(DEVICE->CreateBuffer(&desc, nullptr, m_input.GetAddressOf()));
}

void StructuredBuffer::CreateSRV()
{
	D3D11_BUFFER_DESC desc;
	m_input->GetDesc(&desc);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	srvDesc.BufferEx.NumElements = m_inputCount;

	CHECK(DEVICE->CreateShaderResourceView(m_input.Get(), &srvDesc, m_srv.GetAddressOf()));
}

void StructuredBuffer::CreateOutput()
{
	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.ByteWidth = GetOutputByteWidth();
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = m_outputStride;

	CHECK(DEVICE->CreateBuffer(&desc, nullptr, m_output.GetAddressOf()));
}

void StructuredBuffer::CreateUAV()
{
	D3D11_BUFFER_DESC desc;
	m_output->GetDesc(&desc);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.NumElements = m_outputCount;

	CHECK(DEVICE->CreateUnorderedAccessView(m_output.Get(), &uavDesc, m_uav.GetAddressOf()));
}


void StructuredBuffer::CreateResult()
{
	D3D11_BUFFER_DESC desc;
	m_output->GetDesc(&desc);

	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;

	CHECK(DEVICE->CreateBuffer(&desc, NULL, m_result.GetAddressOf()));
}


void StructuredBuffer::CopyToInput(void* data)
{
	D3D11_MAPPED_SUBRESOURCE subResource;
	DC->Map(m_input.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &subResource);
	{
		memcpy(subResource.pData, data, GetInputByteWidth());
	}
	DC->Unmap(m_input.Get(), 0);
}

void StructuredBuffer::CopyFromOutput(void* data)
{
	DC->CopyResource(m_result.Get(), m_output.Get());

	D3D11_MAPPED_SUBRESOURCE subResource;
	DC->Map(m_result.Get(), 0, D3D11_MAP_READ, 0, &subResource);
	{
		memcpy(data, subResource.pData, GetOutputByteWidth());
	}
	DC->Unmap(m_result.Get(), 0);
}