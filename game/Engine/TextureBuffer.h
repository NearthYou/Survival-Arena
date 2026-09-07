#pragma once

//RawBuffer형식이 아니라. 텍스처를 주고받는 형태. 
class TextureBuffer
{
public:
	TextureBuffer(ComPtr<ID3D11Texture2D> _src);
	~TextureBuffer();


public:
	void CreateBuffer();

private:
	void CreateInput(ComPtr<ID3D11Texture2D> _src);
	void CreateSRV();
	void CreateOutput();
	void CreateUAV();
	void CreateResult();


public:
	uint32 GetWidth() { return m_width; }
	uint32 GetHeight() { return m_height; }
	uint32 GetArraySize() { return m_arraySize; }

	ComPtr<ID3D11Texture2D> GetOutput() { return (ID3D11Texture2D*)m_output.Get(); }
	ComPtr<ID3D11ShaderResourceView> GetOutputSRV() { return m_outputSRV; }

public:
	ComPtr<ID3D11ShaderResourceView> GetSRV() { return m_srv; }
	ComPtr<ID3D11UnorderedAccessView> GetUAV() { return m_uav; }

private:
	ComPtr<ID3D11Texture2D> m_input;
	ComPtr<ID3D11ShaderResourceView> m_srv; // Input
	ComPtr<ID3D11Texture2D> m_output;
	ComPtr<ID3D11UnorderedAccessView> m_uav; // Output

private:
	uint32 m_width = 0;
	uint32 m_height = 0;
	uint32 m_arraySize = 0;
	DXGI_FORMAT m_format;
	ComPtr<ID3D11ShaderResourceView> m_outputSRV;
};

