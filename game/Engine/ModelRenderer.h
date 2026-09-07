#pragma once
#include "Renderer.h"

class Model;
class Shader;
class Material;

class ModelRenderer :
    public Renderer
{
    using Super = Renderer;


public:
    ModelRenderer(shared_ptr<Shader> _shader);
    virtual ~ModelRenderer();

    //virtual void Update() override;

    void SetModel(shared_ptr<Model> _model);
    void SetPass(uint8 _pass) { m_pass = _pass; }

    void RenderInstancing(shared_ptr<class InstancingBuffer>& _buffer, bool _isShadowTech);
    void RenderInstancingDeferred(shared_ptr<class InstancingBuffer>& _buffer, bool _isShadowTech);

    shared_ptr<Model> GetModel() { return m_model; }

    InstanceID GetInstanceID();
#if defined(DXA_GAME_PROBE_DIAGNOSTICS)
    void SetDiagnosticSingleDraw(bool value) { m_diagnosticSingleDraw = value; }
    shared_ptr<Shader> GetDiagnosticShader() { return m_shader; }
    uint8 GetDiagnosticPass() const { return m_pass; }
private:
    bool m_diagnosticSingleDraw = false;
public:
#endif

   
private:
    shared_ptr<Shader>  m_shader;
    uint8               m_pass = 0;
    shared_ptr<Model>   m_model;
};

