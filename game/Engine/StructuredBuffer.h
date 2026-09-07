#pragma once
class StructuredBuffer
{
public:
	StructuredBuffer(void* _inputData, uint32 _inputStride, uint32 _inputCount, uint32 _outputStride = 0, uint32 _outputCount = 0);
	~StructuredBuffer();

public:
	void CreateBuffer();

private:
	void CreateInput();
	void CreateSRV();
	void CreateOutput();
	void CreateUAV();
	void CreateResult();

public:
	uint32 GetInputByteWidth() { return m_inputStride * m_inputCount; }
	uint32 GetOutputByteWidth() { return m_outputStride * m_outputCount; }

	void CopyToInput(void* _data);
	void CopyFromOutput(void* _data);

public:
	ComPtr<ID3D11ShaderResourceView> GetSRV() { return m_srv; }
	ComPtr<ID3D11UnorderedAccessView> GetUAV() { return m_uav; }

private:
	ComPtr<ID3D11Buffer> m_input;
	ComPtr<ID3D11ShaderResourceView> m_srv; // Input
	ComPtr<ID3D11Buffer> m_output;
	ComPtr<ID3D11UnorderedAccessView> m_uav; // Output
	ComPtr<ID3D11Buffer> m_result;

private:
	void* m_inputData;

	//구조체 크기, 카운트
	uint32 m_inputStride = 0;
	uint32 m_inputCount = 0;
	uint32 m_outputStride = 0;
	uint32 m_outputCount = 0;
};

