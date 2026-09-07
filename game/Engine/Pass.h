#pragma once

struct StateBlock
{
	ComPtr<ID3D11RasterizerState> m_RSRasterizerState;
	ComPtr<ID3D11BlendState> m_OMBlendState;
	FLOAT m_OMBlendFactor[4];
	UINT m_OMSampleMask;
	ComPtr<ID3D11DepthStencilState> m_OMDepthStencilState;
	UINT m_OMStencilRef;
};

struct Pass
{
	wstring m_name;
	ID3DX11EffectPass* m_pass;
	D3DX11_PASS_DESC m_desc;

	ComPtr<ID3D11InputLayout> m_inputLayout;
	D3DX11_PASS_SHADER_DESC m_passVsDesc;
	D3DX11_EFFECT_SHADER_DESC m_effectVsDesc;
	vector<D3D11_SIGNATURE_PARAMETER_DESC> m_signatureDescs;

	D3DX11_STATE_BLOCK_MASK m_stateblockMask;
	shared_ptr<StateBlock> m_stateBlock;

	void Draw(UINT _vertexCount, UINT _startVertexLocation = 0);
	void DrawIndexed(UINT _indexCount, UINT _startIndexLocation = 0, INT _baseVertexLocation = 0);
	void DrawInstanced(UINT _vertexCountPerInstance, UINT _instanceCount, UINT _startVertexLocation = 0, UINT _startInstanceLocation = 0);
	void DrawIndexedInstanced(UINT _indexCountPerInstance, UINT _instanceCount, UINT _startIndexLocation = 0, INT _baseVertexLocation = 0, UINT _startInstanceLocation = 0);

	void BeginDraw();
	void EndDraw();

	void Dispatch(UINT _x, UINT _y, UINT _z);
};
