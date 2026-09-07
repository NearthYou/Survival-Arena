#include "pch.h"
#include "Pass.h"
#if defined(DXA_GAME_PROBE_DIAGNOSTICS)
#include "ReferenceEngineCounters.hpp"
#endif

void Pass::Draw(UINT _vertexCount, UINT _startVertexLocation)
{
	BeginDraw();
	{
#if defined(DXA_GAME_PROBE_DIAGNOSTICS)
        ReferenceCountDraw(_vertexCount);
#endif
		DC->Draw(_vertexCount, _startVertexLocation);
	}
	EndDraw();
}

void Pass::DrawIndexed(UINT _indexCount, UINT _startIndexLocation, INT _baseVertexLocation)
{
	BeginDraw();
	{
#if defined(DXA_GAME_PROBE_DIAGNOSTICS)
        ReferenceCountDraw(_indexCount);
#endif
		DC->DrawIndexed(_indexCount, _startIndexLocation, _baseVertexLocation);
	}
	EndDraw();
}

void Pass::DrawInstanced(UINT _vertexCountPerInstance, UINT _instanceCount, UINT _startVertexLocation, UINT _startInstanceLocation)
{
	BeginDraw();
	{
#if defined(DXA_GAME_PROBE_DIAGNOSTICS)
        ReferenceCountDraw(_vertexCountPerInstance, _instanceCount);
#endif
		DC->DrawInstanced(_vertexCountPerInstance, _instanceCount, _startVertexLocation, _startInstanceLocation);
	}
	EndDraw();
}

void Pass::DrawIndexedInstanced(UINT _indexCountPerInstance, UINT _instanceCount, UINT _startIndexLocation, INT _baseVertexLocation, UINT _startInstanceLocation)
{
	BeginDraw();
	{
#if defined(DXA_GAME_PROBE_DIAGNOSTICS)
        ReferenceCountDraw(_indexCountPerInstance, _instanceCount);
#endif
		DC->DrawIndexedInstanced(_indexCountPerInstance, _instanceCount, _startIndexLocation, _baseVertexLocation, _startInstanceLocation);
	}
	EndDraw();
}

void Pass::BeginDraw()
{
	m_pass->ComputeStateBlockMask(&m_stateblockMask);

	DC->IASetInputLayout(m_inputLayout.Get());
	m_pass->Apply(0, DC.Get());
}

void Pass::EndDraw()
{
	if (m_stateblockMask.RSRasterizerState == 1)
		DC->RSSetState(m_stateBlock->m_RSRasterizerState.Get());

	if (m_stateblockMask.OMDepthStencilState == 1)
		DC->OMSetDepthStencilState(m_stateBlock->m_OMDepthStencilState.Get(), m_stateBlock->m_OMStencilRef);

	if (m_stateblockMask.OMBlendState == 1)
		DC->OMSetBlendState(m_stateBlock->m_OMBlendState.Get(), m_stateBlock->m_OMBlendFactor, m_stateBlock->m_OMSampleMask);

	DC->HSSetShader(NULL, NULL, 0);
	DC->DSSetShader(NULL, NULL, 0);
	DC->GSSetShader(NULL, NULL, 0);
}

void Pass::Dispatch(UINT _x, UINT _y, UINT _z)
{
	m_pass->Apply(0, DC.Get());
	DC->Dispatch(_x, _y, _z);

	ID3D11ShaderResourceView* null[1] = { 0 };
	DC->CSSetShaderResources(0, 1, null);

	ID3D11UnorderedAccessView* nullUav[1] = { 0 };
	DC->CSSetUnorderedAccessViews(0, 1, nullUav, NULL);

	DC->CSSetShader(NULL, NULL, 0);
}