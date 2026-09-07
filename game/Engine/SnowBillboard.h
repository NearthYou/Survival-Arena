#pragma once
#include "Renderer.h"

struct VertexSnow {
    Vec3 pos;
    Vec2 uv;
    Vec2 scale;
    Vec2 random;
};

#define MAX_BILLBOARD_COUNT 500

class SnowBillboard :
    public Renderer
{
    using Super = Renderer;

public:
    SnowBillboard(Vec3 _start, Vec3 _extent, int32 _drawCount = 100);
    virtual ~SnowBillboard();

    void InnerRender(bool _isShadowTech) override;
    void SetPass(uint8 _pass) { m_pass = _pass; }
    void SetMaterial(shared_ptr<Material> _material) override;

    void SetParticleScale(float _scale);
    void SetParticleScale(Vec2 _scale);
    void SetColor(Vec4 _color);
    void SetSpiral(float _value) { m_desc.spiralCoef = _value; }

private:
    vector<VertexSnow> m_vertices;
    vector<uint32> m_indices;
    shared_ptr<VertexBuffer> m_vertexBuffer;
    shared_ptr<IndexBuffer> m_indexBuffer;

    int32 m_drawCount = 0;
    uint8 m_pass = 0;

    SnowBillboardDesc m_desc;
    float m_elapsedTime = 0.f;
};

