#pragma once
#include "IExecute.h"

class TextureBufferDemo : public IExecute
{

public:
	void Init() override;
	void Update() override;
	void Render() override;

private:
	shared_ptr<Shader> m_shader;


private:
	ComPtr<ID3D11ShaderResourceView> MakeComputeShaderTexture();
};

