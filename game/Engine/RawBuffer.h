#pragma once

//입력을 받아주고, ComputeShader에 넣어주고., 
class RawBuffer
{

public:
	RawBuffer(void* _inputData, uint32 _inputByte, uint32 _outputByte);
	~RawBuffer();

public:
	void CreateBuffer();
	void CopyToInput(void* data);
	void CopyFromOutput(void* data);

public:
	ComPtr<ID3D11ShaderResourceView> GetSRV() { return m_srv; }
	ComPtr<ID3D11UnorderedAccessView> GetUAV() { return m_uav; }

public:
	void CreateInput();
	void CreateSRV();
	void CreateOutput();
	void CreateUAV();
	void CreateResult();

private:
	ComPtr<ID3D11Buffer> m_input;
	ComPtr<ID3D11ShaderResourceView> m_srv;
	ComPtr<ID3D11Buffer> m_output;
	ComPtr<ID3D11UnorderedAccessView> m_uav;
	ComPtr<ID3D11Buffer> m_result;

private:
	void* m_inputData;
	uint32 m_inputByte = 0;
	uint32 m_outputByte = 0;
};

