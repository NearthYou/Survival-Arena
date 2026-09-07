#pragma once
#include "Pass.h"

struct Technique
{
	wstring m_name;
	D3DX11_TECHNIQUE_DESC m_desc;
	ComPtr<ID3DX11EffectTechnique> m_technique;
	vector<Pass> m_passes;

	void Draw(UINT pass, UINT vertexCount, UINT startVertexLocation = 0);
	void DrawIndexed(UINT pass, UINT indexCount, UINT startIndexLocation = 0, INT baseVertexLocation = 0);
	void DrawInstanced(UINT pass, UINT vertexCountPerInstance, UINT instanceCount, UINT startVertexLocation = 0, UINT startInstanceLocation = 0);
	void DrawIndexedInstanced(UINT pass, UINT indexCountPerInstance, UINT instanceCount, UINT startIndexLocation = 0, INT baseVertexLocation = 0, UINT startInstanceLocation = 0);

	void Dispatch(UINT pass, UINT x, UINT y, UINT z);
};
