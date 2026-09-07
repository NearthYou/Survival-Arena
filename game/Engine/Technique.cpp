#include "pch.h"
#include "Technique.h"

void Technique::Draw(UINT pass, UINT vertexCount, UINT startVertexLocation)
{
	m_passes[pass].Draw(vertexCount, startVertexLocation);
}

void Technique::DrawIndexed(UINT pass, UINT indexCount, UINT startIndexLocation, INT baseVertexLocation)
{
	m_passes[pass].DrawIndexed(indexCount, startIndexLocation, baseVertexLocation);
}

void Technique::DrawInstanced(UINT pass, UINT vertexCountPerInstance, UINT instanceCount, UINT startVertexLocation, UINT startInstanceLocation)
{
	m_passes[pass].DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
}

void Technique::DrawIndexedInstanced(UINT pass, UINT indexCountPerInstance, UINT instanceCount, UINT startIndexLocation, INT baseVertexLocation, UINT startInstanceLocation)
{
	m_passes[pass].DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
}

void Technique::Dispatch(UINT pass, UINT x, UINT y, UINT z)
{
	m_passes[pass].Dispatch(x, y, z);
}