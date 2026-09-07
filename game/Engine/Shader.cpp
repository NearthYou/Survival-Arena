#include "pch.h"
#include "Shader.h"
#include "Utils.h"
#include "Camera.h"

Shader::Shader(wstring _file) : m_file(L"..\\Shaders\\" + _file)
{
	m_initialStateBlock = make_shared<StateBlock>();
	{
		DC->RSGetState(m_initialStateBlock->m_RSRasterizerState.GetAddressOf());
		DC->OMGetBlendState(m_initialStateBlock->m_OMBlendState.GetAddressOf(), m_initialStateBlock->m_OMBlendFactor, &m_initialStateBlock->m_OMSampleMask);
		DC->OMGetDepthStencilState(m_initialStateBlock->m_OMDepthStencilState.GetAddressOf(), &m_initialStateBlock->m_OMStencilRef);
	}

	CreateEffect();
}

Shader::~Shader()
{
	m_globalBuffer.reset();
	m_transformBuffer.reset();
	m_lightBuffer.reset();
	m_materialBuffer.reset();
	m_boneBuffer.reset();
	m_keyframeBuffer.reset();
	m_tweenBuffer.reset();
	m_snowBuffer.reset();
	m_particleBuffer.reset();
	m_shadowBuffer.reset();
	m_textBuffer.reset();

	m_globalEffectBuffer.Reset();
	m_transformEffectBuffer.Reset();
	m_lightEffectBuffer.Reset();
	m_materialEffectBuffer.Reset();
	m_boneEffectBuffer.Reset();
	m_keyframeEffectBuffer.Reset();
	m_tweenEffectBuffer.Reset();
	m_snowEffectBuffer.Reset();
	m_particleEffectBuffer.Reset();
	m_shadowEffectBuffer.Reset();
	m_textEffectBuffer.Reset();

	for (auto& technique : m_techniques) {
		technique.m_technique.Reset();
		for (auto& pass : technique.m_passes) {
			pass.m_inputLayout.Reset();
		}
	}
	m_techniques.clear();
}


void Shader::CreateEffect()
{
	m_shaderDesc = ShaderManager::GetEffect(m_file);

	m_shaderDesc.m_effect->GetDesc(&m_effectDesc);
	for (UINT t = 0; t < m_effectDesc.Techniques; ++t)
	{
		Technique technique;
		technique.m_technique = m_shaderDesc.m_effect->GetTechniqueByIndex(t);
		technique.m_technique->GetDesc(&technique.m_desc);
		technique.m_name = Utils::ToWString(technique.m_desc.Name);

		for (UINT p = 0; p < technique.m_desc.Passes; ++p)
		{
			Pass pass;
			pass.m_pass = technique.m_technique->GetPassByIndex(p);
			pass.m_pass->GetDesc(&pass.m_desc);
			pass.m_name = Utils::ToWString(pass.m_desc.Name);
			pass.m_pass->GetVertexShaderDesc(&pass.m_passVsDesc);
			pass.m_passVsDesc.pShaderVariable->GetShaderDesc(pass.m_passVsDesc.ShaderIndex, &pass.m_effectVsDesc);

			for (UINT s = 0; s < pass.m_effectVsDesc.NumInputSignatureEntries; ++s)
			{
				D3D11_SIGNATURE_PARAMETER_DESC desc;

				HRESULT hr = pass.m_passVsDesc.pShaderVariable->GetInputSignatureElementDesc(pass.m_passVsDesc.ShaderIndex, s, &desc);
				CHECK(hr);

				pass.m_signatureDescs.push_back(desc);
			}

			pass.m_inputLayout = CreateInputLayout(m_shaderDesc.m_blob, &pass.m_effectVsDesc, pass.m_signatureDescs);
			pass.m_stateBlock = m_initialStateBlock;


			technique.m_passes.push_back(pass);
		}
		m_techniqueMap[technique.m_name] = static_cast<int>(t);
		m_techniques.push_back(technique);
	}

	for (UINT i = 0; i < m_effectDesc.ConstantBuffers; ++i)
	{
		ID3DX11EffectConstantBuffer* iBuffer;
		iBuffer = m_shaderDesc.m_effect->GetConstantBufferByIndex(i);

		D3DX11_EFFECT_VARIABLE_DESC vDesc;
		iBuffer->GetDesc(&vDesc);
	}

	for (UINT i = 0; i < m_effectDesc.GlobalVariables; ++i)
	{
		ID3DX11EffectVariable* effectVariable;
		effectVariable = m_shaderDesc.m_effect->GetVariableByIndex(i);

		D3DX11_EFFECT_VARIABLE_DESC vDesc;
		effectVariable->GetDesc(&vDesc);
	}
}

ComPtr<ID3D11InputLayout> Shader::CreateInputLayout(ComPtr<ID3DBlob> _fxBlob, D3DX11_EFFECT_SHADER_DESC* _effectVsDesc, vector<D3D11_SIGNATURE_PARAMETER_DESC>& _params)
{
	std::vector<D3D11_INPUT_ELEMENT_DESC> inputLayoutDesc;

	for (D3D11_SIGNATURE_PARAMETER_DESC& paramDesc : _params)
	{
		D3D11_INPUT_ELEMENT_DESC elementDesc;
		elementDesc.SemanticName = paramDesc.SemanticName;
		elementDesc.SemanticIndex = paramDesc.SemanticIndex;
		elementDesc.InputSlot = 0;
		elementDesc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
		elementDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		elementDesc.InstanceDataStepRate = 0;

		if (paramDesc.Mask == 1)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
				elementDesc.Format = DXGI_FORMAT_R32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
				elementDesc.Format = DXGI_FORMAT_R32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
				elementDesc.Format = DXGI_FORMAT_R32_FLOAT;
		}
		else if (paramDesc.Mask <= 3)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
				elementDesc.Format = DXGI_FORMAT_R32G32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
				elementDesc.Format = DXGI_FORMAT_R32G32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
				elementDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		}
		else if (paramDesc.Mask <= 7)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
				elementDesc.Format = DXGI_FORMAT_R32G32B32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
				elementDesc.Format = DXGI_FORMAT_R32G32B32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
				elementDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		}
		else if (paramDesc.Mask <= 15)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
				elementDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
				elementDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
				elementDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		}

		string name = paramDesc.SemanticName;
		std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::toupper(c); });

		if (name == "POSITION")
		{
			elementDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		}

		if (Utils::StartsWith(name, "INST") == true)
		{
			elementDesc.InputSlot = 1;
			elementDesc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			elementDesc.InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
			elementDesc.InstanceDataStepRate = 1;
		}

		if (Utils::StartsWith(name, "SV_") == false)
			inputLayoutDesc.push_back(elementDesc);
	}

	const void* code = _effectVsDesc->pBytecode;
	UINT codeSize = _effectVsDesc->BytecodeLength;


	if (inputLayoutDesc.size() > 0)
	{
		ComPtr<ID3D11InputLayout> inputLayout;

		HRESULT hr = DEVICE->CreateInputLayout
		(
			&inputLayoutDesc[0]
			, inputLayoutDesc.size()
			, code
			, codeSize
			, inputLayout.GetAddressOf()
		);

		CHECK(hr);

		return inputLayout;
	}

	return nullptr;
}

void Shader::Draw(UINT _technique, UINT _pass, UINT _vertexCount, UINT _startVertexLocation)
{
	m_techniques[_technique].m_passes[_pass].Draw(_vertexCount, _startVertexLocation);
}

void Shader::DrawIndexed(UINT _technique, UINT _pass, UINT _indexCount, UINT _startIndexLocation, INT _baseVertexLocation)
{
	m_techniques[_technique].m_passes[_pass].DrawIndexed(_indexCount, _startIndexLocation, _baseVertexLocation);
}

void Shader::DrawInstanced(UINT _technique, UINT _pass, UINT _vertexCountPerInstance, UINT _instanceCount, UINT _startVertexLocation, UINT _startInstanceLocation)
{
	m_techniques[_technique].m_passes[_pass].DrawInstanced(_vertexCountPerInstance, _instanceCount, _startVertexLocation, _startInstanceLocation);
}

void Shader::DrawIndexedInstanced(UINT _technique, UINT _pass, UINT _indexCountPerInstance, UINT _instanceCount, UINT _startIndexLocation, INT _baseVertexLocation, UINT _startInstanceLocation)
{
	m_techniques[_technique].m_passes[_pass].DrawIndexedInstanced(_indexCountPerInstance, _instanceCount, _startIndexLocation, _baseVertexLocation, _startInstanceLocation);
}

void Shader::DrawIndexedInstancedCurTech(UINT _pass, UINT _indexCountPerInstance, UINT _instanceCount, UINT _startIndexLocation, INT _baseVertexLocation, UINT _startInstanceLocation)
{
	auto currentTech = GetCurrentTechnique();
	if (currentTech)
	{
		currentTech->DrawIndexedInstanced(_pass, _indexCountPerInstance, _instanceCount,
			_startIndexLocation, _baseVertexLocation, _startInstanceLocation);
	}
}


void Shader::BeginDraw(UINT _technique, UINT _pass)
{
	m_techniques[_technique].m_passes[_pass].BeginDraw();
}

void Shader::EndDraw(UINT _technique, UINT _pass)
{
	m_techniques[_technique].m_passes[_pass].EndDraw();
}

void Shader::Dispatch(UINT _technique, UINT _pass, UINT _x, UINT _y, UINT _z)
{
	m_techniques[_technique].m_passes[_pass].Dispatch(_x, _y, _z);
}

ComPtr<ID3DX11EffectVariable> Shader::GetVariable(string _name)
{
	return m_shaderDesc.m_effect->GetVariableByName(_name.c_str());
}

ComPtr<ID3DX11EffectScalarVariable> Shader::GetScalar(string _name)
{
	return m_shaderDesc.m_effect->GetVariableByName(_name.c_str())->AsScalar();
}

ComPtr<ID3DX11EffectVectorVariable> Shader::GetVector(string _name)
{
	return m_shaderDesc.m_effect->GetVariableByName(_name.c_str())->AsVector();
}

ComPtr<ID3DX11EffectMatrixVariable> Shader::GetMatrix(string _name)
{
	return m_shaderDesc.m_effect->GetVariableByName(_name.c_str())->AsMatrix();
}

ComPtr<ID3DX11EffectStringVariable> Shader::GetString(string _name)
{
	return m_shaderDesc.m_effect->GetVariableByName(_name.c_str())->AsString();
}

ComPtr<ID3DX11EffectShaderResourceVariable> Shader::GetSRV(string _name)
{
	return m_shaderDesc.m_effect->GetVariableByName(_name.c_str())->AsShaderResource();
}

ComPtr<ID3DX11EffectRenderTargetViewVariable> Shader::GetRTV(string _name)
{
	return m_shaderDesc.m_effect->GetVariableByName(_name.c_str())->AsRenderTargetView();
}

ComPtr<ID3DX11EffectDepthStencilViewVariable> Shader::GetDSV(string _name)
{
	return m_shaderDesc.m_effect->GetVariableByName(_name.c_str())->AsDepthStencilView();
}

ComPtr<ID3DX11EffectConstantBuffer> Shader::GetConstantBuffer(string _name)
{
	return m_shaderDesc.m_effect->GetConstantBufferByName(_name.c_str());
}

ComPtr<ID3DX11EffectShaderVariable> Shader::GetShader(string _name)
{
	return m_shaderDesc.m_effect->GetVariableByName(_name.c_str())->AsShader();
}

ComPtr<ID3DX11EffectBlendVariable> Shader::GetBlend(string _name)
{
	return m_shaderDesc.m_effect->GetVariableByName(_name.c_str())->AsBlend();
}

ComPtr<ID3DX11EffectDepthStencilVariable> Shader::GetDepthStencil(string _name)
{
	return m_shaderDesc.m_effect->GetVariableByName(_name.c_str())->AsDepthStencil();
}

ComPtr<ID3DX11EffectRasterizerVariable> Shader::GetRasterizer(string _name)
{
	return m_shaderDesc.m_effect->GetVariableByName(_name.c_str())->AsRasterizer();
}

ComPtr<ID3DX11EffectSamplerVariable> Shader::GetSampler(string _name)
{
	return m_shaderDesc.m_effect->GetVariableByName(_name.c_str())->AsSampler();
}

void Shader::SetTechnique(const wstring& _name)
{
	auto it = m_techniqueMap.find(_name);
	if (it != m_techniqueMap.end())
	{
		m_currentTechniqueIndex = it->second;
	}
	else
	{
		OutputDebugStringA("Technique not found: ");
	}
}

void Shader::SetTechnique(int _index)
{
	if (_index >= 0 && _index < m_techniques.size())
	{
		m_currentTechniqueIndex = _index;
	}
}

Technique* Shader::GetCurrentTechnique()
{
	if (m_currentTechniqueIndex >= 0 && m_currentTechniqueIndex < m_techniques.size())
	{
		return &m_techniques[m_currentTechniqueIndex];
	}
	return nullptr;
}

Technique* Shader::GetTechnique(const wstring& _name)
{
	auto iter = m_techniqueMap.find(_name);
	if (iter != m_techniqueMap.end()) {
		return GetTechnique(iter->second);
	}
	else {
		return nullptr;
	}
}

Technique* Shader::GetTechnique(int _idx)
{
	if (_idx >= 0 && _idx < m_techniques.size())
	{
		return &m_techniques[_idx];
	}
	else {
		return nullptr;
	}
}


ComPtr<ID3DX11EffectUnorderedAccessViewVariable> Shader::GetUAV(string _name)
{
	return m_shaderDesc.m_effect->GetVariableByName(_name.c_str())->AsUnorderedAccessView();
}

unordered_map<wstring, ShaderDesc> ShaderManager::m_shaders;

ShaderDesc ShaderManager::GetEffect(wstring _fileName)
{
	if (m_shaders.count(_fileName) == 0)
	{
		ComPtr<ID3DBlob> blob;
		ComPtr<ID3DBlob> error;
		//INT flag = D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY | D3D10_SHADER_PACK_MATRIX_ROW_MAJOR;

		//HRESULT hr = ::D3DCompileFromFile(_fileName.c_str(), NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, NULL, "fx_5_0", flag, NULL, blob.GetAddressOf(), error.GetAddressOf());
		
		WORD shaderFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
		shaderFlags |= D3D10_SHADER_DEBUG;
		shaderFlags |= D3D10_SHADER_SKIP_OPTIMIZATION;
#endif

		shaderFlags |= D3D10_SHADER_PACK_MATRIX_ROW_MAJOR;
		HRESULT hr = ::D3DCompileFromFile(_fileName.c_str(), NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, NULL, "fx_5_0", shaderFlags, NULL, blob.GetAddressOf(), error.GetAddressOf());

		if (FAILED(hr))
		{
			if (error != NULL)
			{
				string str = (const char*)error->GetBufferPointer();
				MessageBoxA(NULL, str.c_str(), "Shader Error", MB_OK);
			}
			assert(false);
		}

		ComPtr<ID3DX11Effect> effect;
		hr = ::D3DX11CreateEffectFromMemory(blob->GetBufferPointer(), blob->GetBufferSize(), 0, DEVICE.Get(), effect.GetAddressOf());
		CHECK(hr);
		
		m_shaders[_fileName] = ShaderDesc{blob, effect};
	}
	
	ShaderDesc desc = m_shaders.at(_fileName);
	ComPtr<ID3DX11Effect> effect;
	desc.m_effect->CloneEffect(D3DX11_EFFECT_CLONE_FORCE_NONSINGLE, effect.GetAddressOf());

	return ShaderDesc{desc.m_blob, effect};
}

void Shader::PushGlobalData(const Matrix& _view, const Matrix& _projection)
{
	if (m_globalBuffer == nullptr) {
		m_globalBuffer = make_shared<ConstantBuffer<GlobalDesc>>();
		m_globalBuffer->Create();
		m_globalEffectBuffer = GetConstantBuffer("GlobalBuffer");
	}
	m_globalDesc.P = _projection;
	m_globalDesc.V = _view;
	m_globalDesc.VP = _view * _projection;
	m_globalDesc.Vinv = _view.Invert();
	m_globalDesc.CamPos = Camera::s_Pos;

	m_globalBuffer->CopyData(m_globalDesc);
	m_globalEffectBuffer->SetConstantBuffer(m_globalBuffer->GetComPtr().Get());

}

void Shader::PushTransformData(const TransformDesc& _desc)
{
	if (m_transformBuffer == nullptr) {
		m_transformBuffer = make_shared<ConstantBuffer<TransformDesc>>();
		m_transformBuffer->Create();
		m_transformEffectBuffer = GetConstantBuffer("TransformBuffer");
	}

	m_transformDesc = _desc;

	m_transformBuffer->CopyData(m_transformDesc);
	m_transformEffectBuffer->SetConstantBuffer(m_transformBuffer->GetComPtr().Get());
}

void Shader::PushLightData(const LightDesc& _desc)
{
	if (m_lightBuffer == nullptr) {
		m_lightBuffer = make_shared<ConstantBuffer<LightDesc>>();
		m_lightBuffer->Create();
		m_lightEffectBuffer = GetConstantBuffer("LightBuffer");
	}

	m_lightDesc = _desc;
	m_lightBuffer->CopyData(m_lightDesc);
	m_lightEffectBuffer->SetConstantBuffer(m_lightBuffer->GetComPtr().Get());
}

void Shader::PushMaterialData(const MaterialDesc& _desc)
{
	if (m_materialBuffer == nullptr) {

		m_materialBuffer = make_shared<ConstantBuffer<MaterialDesc>>();
		m_materialBuffer->Create();
		m_materialEffectBuffer = GetConstantBuffer("MaterialBuffer");
	}

	m_materialDesc = _desc;
	m_materialBuffer->CopyData(m_materialDesc);
	m_materialEffectBuffer->SetConstantBuffer(m_materialBuffer->GetComPtr().Get());
}

void Shader::PushBoneData(const BoneDesc& _desc)
{

	if (m_boneBuffer == nullptr) {
		m_boneBuffer = make_shared<ConstantBuffer<BoneDesc>>();
		m_boneBuffer->Create();
		m_boneEffectBuffer = GetConstantBuffer("BoneBuffer");
	}

	m_boneDesc = _desc;
	m_boneBuffer->CopyData(m_boneDesc);
	m_boneEffectBuffer->SetConstantBuffer(m_boneBuffer->GetComPtr().Get());
}

void Shader::PushKeyframeData(const KeyframeDesc& _desc)
{
	if (m_keyframeBuffer == nullptr) {
		m_keyframeBuffer = make_shared<ConstantBuffer<KeyframeDesc>>();
		m_keyframeBuffer->Create();
		m_keyframeEffectBuffer = GetConstantBuffer("KeyframeBuffer");
	}


	m_keyframeDesc = _desc;
	m_keyframeBuffer->CopyData(m_keyframeDesc);
	m_keyframeEffectBuffer->SetConstantBuffer(m_keyframeBuffer->GetComPtr().Get());
}

void Shader::PushTweenData(const InstancedTweenDesc& _desc)
{
	if (m_tweenBuffer == nullptr) {
		m_tweenBuffer = make_shared<ConstantBuffer<InstancedTweenDesc>>();
		m_tweenBuffer->Create();
		m_tweenEffectBuffer = GetConstantBuffer("TweenBuffer");
	}

	m_tweenDesc = _desc;
	m_tweenBuffer->CopyData(m_tweenDesc);
	m_tweenEffectBuffer->SetConstantBuffer(m_tweenBuffer->GetComPtr().Get());
}

void Shader::PushSnowData(const SnowBillboardDesc& _desc)
{
	if (m_snowBuffer == nullptr) {
		m_snowBuffer = make_shared<ConstantBuffer<SnowBillboardDesc>>();
		m_snowBuffer->Create();
		m_snowEffectBuffer = GetConstantBuffer("SnowBuffer");
	}

	m_snowDesc = _desc;
	m_snowBuffer->CopyData(m_snowDesc);
	m_snowEffectBuffer->SetConstantBuffer(m_snowBuffer->GetComPtr().Get());
}

void Shader::PushParticleData(const ParticleDesc& _desc)
{
	if (m_particleBuffer == nullptr) {
		m_particleBuffer = make_shared<ConstantBuffer<ParticleDesc>>();
		m_particleBuffer->Create();
		m_particleEffectBuffer = GetConstantBuffer("ParticleBuffer");
	}

	m_particleDesc = _desc;
	m_particleBuffer->CopyData(m_particleDesc);
	m_particleEffectBuffer->SetConstantBuffer(m_particleBuffer->GetComPtr().Get());
}

void Shader::PushShadowData(const Matrix& _desc)
{
	if (m_shadowBuffer == nullptr) {
		m_shadowBuffer = make_shared<ConstantBuffer<Matrix>>();
		m_shadowBuffer->Create();
		m_shadowEffectBuffer = GetConstantBuffer("ShadowBuffer");
	}

	m_shadowDesc = _desc;
	m_shadowBuffer->CopyData(m_shadowDesc);
	m_shadowEffectBuffer->SetConstantBuffer(m_shadowBuffer->GetComPtr().Get());
}

void Shader::PushTextData(const Vec4& textColor, const Vec4& outlineColor, float alpha, float outlineWidth)
{
	if (m_textBuffer == nullptr) {
		m_textBuffer = make_shared<ConstantBuffer<TextDesc>>();
		m_textBuffer->Create();
		m_textEffectBuffer = GetConstantBuffer("TextMaterialBuffer");
	}

	m_textDesc.textColor = textColor;
	m_textDesc.outlineColor = outlineColor;
	m_textDesc.textAlpha = alpha;
	m_textDesc.outlineWidth = outlineWidth;
	m_textDesc.textPadding = Vec2(0, 0);

	m_textBuffer->CopyData(m_textDesc);
	m_textEffectBuffer->SetConstantBuffer(m_textBuffer->GetComPtr().Get());
}

void Shader::PushFOWData(const FogOfWarData& _desc)
{
	if (m_fowBuffer == nullptr) {
		m_fowBuffer = make_shared<ConstantBuffer<FogOfWarData>>();
		m_fowBuffer->Create();
		m_fowEffectBuffer = GetConstantBuffer("FogOfWarData");
	}

	m_fowDesc = _desc;
	m_fowBuffer->CopyData(m_fowDesc);
	m_fowEffectBuffer->SetConstantBuffer(m_fowBuffer->GetComPtr().Get());
}

void Shader::PushMultiLightData(const MultiLightDesc& _desc)
{
	if (m_multiLightBuffer == nullptr) {
		m_multiLightBuffer = make_shared<ConstantBuffer<MultiLightDesc>>();
		m_multiLightBuffer->Create();
		m_multiLightEffectBuffer = GetConstantBuffer("MultiLightBuffer");
	}

	m_multiLightDesc = _desc;
	m_multiLightBuffer->CopyData(m_multiLightDesc);
	m_multiLightEffectBuffer->SetConstantBuffer(m_multiLightBuffer->GetComPtr().Get());
}

void Shader::PushOutlineData(const OutlineDesc& _desc)
{
	if (m_OutlineBuffer == nullptr) {
		m_OutlineBuffer = make_shared<ConstantBuffer<OutlineDesc>>();
		m_OutlineBuffer->Create();
		m_OutlineEffectBuffer = GetConstantBuffer("OutlineBuffer");
	}

	m_OutlineDesc = _desc;
	m_OutlineBuffer->CopyData(m_OutlineDesc);
	m_OutlineEffectBuffer->SetConstantBuffer(m_OutlineBuffer->GetComPtr().Get());
}

void Shader::PushDecalData(const DecalBufferData& _desc)
{
	if (m_DecalBuffer == nullptr) {
		m_DecalBuffer = make_shared<ConstantBuffer<DecalBufferData>>();
		m_DecalBuffer->Create();
		m_DecalEffectBuffer = GetConstantBuffer("DecalBuffer");
	}

	m_DecalDesc = _desc;
	m_DecalBuffer->CopyData(m_DecalDesc);
	m_DecalEffectBuffer->SetConstantBuffer(m_DecalBuffer->GetComPtr().Get());
}

// Shader.cpp¿¡ Ãß°¡
void Shader::PushScrollViewClippingData(const Vec4& clippingRect, bool enableClipping)
{
	if (m_scrollViewClippingBuffer == nullptr) {
		m_scrollViewClippingBuffer = make_shared<ConstantBuffer<ScrollViewClippingData>>();
		m_scrollViewClippingBuffer->Create();
		m_scrollViewClippingEffectBuffer = GetConstantBuffer("ScrollViewClippingBuffer");
	}

	m_scrollViewClippingDesc.clippingRect = clippingRect;
	m_scrollViewClippingDesc.enableClipping = enableClipping ? 1.0f : 0.0f;
	m_scrollViewClippingDesc.padding = Vec3(0, 0, 0);

	m_scrollViewClippingBuffer->CopyData(m_scrollViewClippingDesc);
	m_scrollViewClippingEffectBuffer->SetConstantBuffer(m_scrollViewClippingBuffer->GetComPtr().Get());
}

void Shader::PushHealthBarData(float _healthRatio, float _manaRatio, int _type)
{
	if (m_healthBarBuffer == nullptr) {
		m_healthBarBuffer = make_shared<ConstantBuffer<HealthBarData>>();
		m_healthBarBuffer->Create();
		m_healthBarEffectBuffer = GetConstantBuffer("HealthBarBuffer");
	}

	m_healthBarDesc.healthRatio = _healthRatio;
	m_healthBarDesc.manaRatio = _manaRatio;
	m_healthBarDesc.type = _type;
	m_healthBarDesc.padding = 0;
	m_healthBarBuffer->CopyData(m_healthBarDesc);
	m_healthBarEffectBuffer->SetConstantBuffer(m_healthBarBuffer->GetComPtr().Get());
}

void Shader::SetDecalTexture(shared_ptr<Texture> _texture)
{
	m_decalTexture = _texture;

	if (m_decalTextureEffect == nullptr)
		m_decalTextureEffect = GetSRV("DecalTexture");
	
	if (m_decalTexture && m_decalTextureEffect) {
		m_decalTextureEffect->SetResource(m_decalTexture->GetComPtr().Get());
	}
}

void Shader::SetDepthTexture(shared_ptr<Texture> _depthtexture)
{
	m_depthTexture = _depthtexture;

	if (m_depthTextureEffect == nullptr)
		m_depthTextureEffect = GetSRV("DepthTexture");

	if (m_depthTexture && m_depthTextureEffect) {
		m_depthTextureEffect->SetResource(m_depthTexture->GetComPtr().Get());
	}
}

bool Shader::IsFOWShader() const
{
	return m_file.find(L"FOW.fx") != wstring::npos;
}
