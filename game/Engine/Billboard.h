#pragma once
#include "Renderer.h"

struct VertexBillboard {
    Vec3 pos;
    Vec2 uv;
    Vec2 scale;
};

#define MAX_BILLBOARD_COUNT 10000

class Billboard :
    public Renderer
{
    using Super = Renderer;

public:
    Billboard();
    virtual ~Billboard();

    void InnerRender(bool _isShadowTech) override;
    void Add(Vec3 _pos, Vec2 _scale);

    void SetPass(uint8 _pass) { m_pass = _pass; }

private:
    vector<VertexBillboard> m_vertices;
    vector<uint32> m_indices;
    shared_ptr<VertexBuffer> m_vertexBuffer;
    shared_ptr<IndexBuffer> m_indexBuffer;

    int32 m_drawCount = 0;
    int32 m_prevCount = 0;

    uint8 m_pass = 0;
};

