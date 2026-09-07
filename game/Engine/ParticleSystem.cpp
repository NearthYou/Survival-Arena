#include "pch.h"
#include "ParticleSystem.h"
#include "Material.h"
#include "Shader.h"
#include "Windows.h"

ParticleSystem::ParticleSystem() : Super(ComponentType::ParticleSystem)
{
	BuildVB();
}


ParticleSystem::~ParticleSystem() {
	Cleanup();
}

void ParticleSystem::Cleanup() {
	if (_inputLayout) {
		_inputLayout.Reset();
	}

	// VertexBuffer Á¤¸®
	if (_initVB) _initVB.reset();
	if (_drawVB) _drawVB.reset();
	if (_streamOutVB) _streamOutVB.reset();
}

void ParticleSystem::Reset()
{
	_firstRun = true;
	_age = 0.0f;
}

void ParticleSystem::Update()
{
	_emitPosW = GetTransform()->GetPosition();
	_timeStep = TIME->GetDeltaTime(); 
	_gameTime = TIME->GetGameTime();

	_age += _timeStep;
}

void ParticleSystem::InnerRender(bool _isShadowTech)
{
	//Super::Render();

	assert(!_isShadowTech);
	Super::InnerRender(_isShadowTech);

	const shared_ptr<Shader>& shader = m_material->GetShader();


	_desc.timeStep = _timeStep;
	_desc.gameTime = _gameTime;
	_desc.emitDirW = _emitDirW;
	_desc.emitPosW = _emitPosW;
	shader->PushParticleData(_desc);

	DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	uint32 stride = sizeof(ParticleVertex);
	uint32 offset = 0;
	if (_firstRun)
		_initVB->PushData();
	else
		_drawVB->PushData();


	shader->BeginDraw(0, 0);
	DC->SOSetTargets(1, _streamOutVB->GetComPtr().GetAddressOf(), &offset);

	if (_firstRun)
	{
		DC->Draw(1, 0);
		_firstRun = false;
	}
	else
	{
		DC->DrawAuto();
	}

	shader->EndDraw(0, 0);
	std::swap(_drawVB, _streamOutVB);

	ID3D11Buffer* bufferArray[1] = { 0 };

	shader->BeginDraw(1, 0);
	DC->SOSetTargets(1, bufferArray, &offset);

	_drawVB->PushData();

	DC->DrawAuto();
	shader->EndDraw(1, 0);
}

void ParticleSystem::SetMaterial(shared_ptr<Material> material)
{
	Super::SetMaterial(material);
	material->SetRandomTex(RESOURCES->Get<Texture>(L"RandomTex"));
	material->SetCastShadow(false);
}

void ParticleSystem::BuildVB()
{
	ParticleVertex p;
	ZeroMemory(&p, sizeof(ParticleVertex));
	p.Age = 0.0f;
	p.Type = 0;

	vector<ParticleVertex> vertices;
	vertices.push_back(p);
	_initVB = make_shared<VertexBuffer>();
	_initVB->Create(vertices, 0, false, true);

	_drawVB = make_shared<VertexBuffer>();
	_drawVB->CreateStreamOut<ParticleVertex>(MAX_PARTICLES);

	_streamOutVB = make_shared<VertexBuffer>();
	_streamOutVB->CreateStreamOut<ParticleVertex>(MAX_PARTICLES);
}
