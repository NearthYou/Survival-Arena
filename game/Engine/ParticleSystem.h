#pragma once
#include "Renderer.h"

#define MAX_PARTICLES 10000

struct ParticleVertex
{
	Vec3 InitialPos; 
	Vec3 InitialVel;
	Vec2 Size;
	float Age;
	uint32 Type;
};
class ParticleSystem : public Renderer
{
    using Super = Renderer;
public:
    ParticleSystem();
    virtual ~ParticleSystem();
	void Cleanup(); // 명시적 정리 함수


	void Reset(); 

	void Update() override;
	void InnerRender(bool _isShadowTech) override;

	void SetMaterial(shared_ptr<Material> material) override;

	void SetEmitPosW(Vec3 emitPosW) { _emitPosW = emitPosW; }
	void SetEmitDirW(Vec3 emitDirW) { _emitDirW = emitDirW; }

private:
	void BuildVB();

private:
	ParticleDesc _desc;
	bool _firstRun;
	float _age;
	float _timeStep;
	float _gameTime;
	Vec3 _emitPosW;
	Vec3 _emitDirW;

	//ComPtr<ID3D11Buffer> _initVB;
	//ComPtr<ID3D11Buffer> _drawVB;
	//ComPtr<ID3D11Buffer> _streamOutVB;
	shared_ptr<VertexBuffer> _initVB;
	shared_ptr<VertexBuffer> _drawVB;
	shared_ptr<VertexBuffer> _streamOutVB;

	ComPtr<ID3D11InputLayout> _inputLayout;
};


const D3D11_INPUT_ELEMENT_DESC ParticleInputDesc[5] = 
{
	{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"VELOCITY", 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"SIZE",     0, DXGI_FORMAT_R32G32_FLOAT,		0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"AGE",      0, DXGI_FORMAT_R32_FLOAT,			0, 34, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TYPE",     0, DXGI_FORMAT_R32_UINT,			0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0},
};
