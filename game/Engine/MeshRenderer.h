#pragma once
#include "Renderer.h"

class Mesh;
class Shader;
class Material;

#define MAX_MESH_INSTANCE 500

class MeshRenderer : public Renderer
{
	using Super = Renderer;
public:
	MeshRenderer();
	virtual ~MeshRenderer();

	//virtual void Update() override;

	shared_ptr<Mesh> GetMesh() { return m_mesh; }
	void SetMesh(shared_ptr<Mesh> _mesh) { m_mesh = _mesh; }

	void RenderInstancing(shared_ptr<class InstancingBuffer>& _buffer, bool _isShadowTech);
	void RenderInstancingDeferred(shared_ptr<class InstancingBuffer>& _buffer, bool _isShadowTech);
	InstanceID GetInstanceID();

private:
	shared_ptr<Mesh> m_mesh;
	//shared_ptr<Texture> m_texture;
	//shared_ptr<Shader> m_shader;
	//텍스처랑 셰이더는 머테리얼 안으로 들어감. 


};

