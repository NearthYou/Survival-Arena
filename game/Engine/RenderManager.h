#pragma once

#include "InstancingBuffer.h"
#include "BindShaderDesc.h"

class GameObject;
struct FogOfWarData;

class RenderManager
{
	DECLARE_SINGLE(RenderManager);


private:
	FogOfWarData m_FogData;
public:

	void Init();
	void Render(vector<shared_ptr<GameObject>>& _gameObjects, bool _isShadowTech);
	void RenderForward(vector<shared_ptr<GameObject>>& _gameObjects, bool _isShadowTech);
	void RenderDeferred(vector<shared_ptr<GameObject>>& _gameObjects, bool _isShadowTech);


	void Clear() { m_buffers.clear(); }
	void ClearData();

	void SetDeferredLightingShader(shared_ptr<Shader> _shader) {
		m_deferredLightingShader = _shader;
	}

	void SetDeferredRendering(bool _enable) {
		m_useDeferredRendering = _enable;
	}

	void SetFOWData(FogOfWarData& _data) {
		m_FogData = _data;
	}

private:

	void RenderMeshRendererForward(vector<shared_ptr<GameObject>>& _gameObjects);
	void RenderModelRendererForward(vector<shared_ptr<GameObject>>& _gameObjects);
	void RenderAnimRendererForward(vector<shared_ptr<GameObject>>& _gameObjects);

	void RenderMeshRendererDeferred(vector<shared_ptr<GameObject>>& _gameObjects);
	void RenderModelRendererDeferred(vector<shared_ptr<GameObject>>& _gameObjects);
	void RenderAnimRendererDeferred(vector<shared_ptr<GameObject>>& _gameObjects);

	void RenderGeometryPass(vector<shared_ptr<GameObject>>& _gameObjects);
	void RenderDeferredLighting();
	void RenderOutlinePostProcess();

	void RenderTransparentObjects(vector<shared_ptr<GameObject>>& _gameObjects);
	void RenderDecals(vector<shared_ptr<GameObject>>& _gameObjects);
private:
	void AddData(InstanceID _instanceID, InstancingData& _data);

private:
	map<InstanceID, shared_ptr<InstancingBuffer>> m_buffers;
	bool m_isShadowTech = false;
	bool m_useDeferredRendering = true;

	shared_ptr<Shader> m_deferredLightingShader;
	shared_ptr<Shader> m_outlineShader;
};

