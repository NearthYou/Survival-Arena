#include "pch.h"
#include "RenderManager.h"
#include "InstancingBuffer.h"
#include "GameObject.h"
#include "MeshRenderer.h"
#include "ModelRenderer.h"
#include "ModelAnimator.h"
#include "Transform.h"
#include "Camera.h"
#include "ParticleSystem.h"
#include "Billboard.h"
#include "SnowBillboard.h"
#include "Material.h"
#include "Light.h"

class GameObject;

void RenderManager::Init()
{
	m_deferredLightingShader = make_shared<Shader>(L"FOW.fx");
	m_deferredLightingShader->SetTechnique(L"DeferredLightingTech");

	m_outlineShader = make_shared<Shader>(L"OutlinePostProcess.fx");
	// 디퍼드 렌더링 활성화
	SetDeferredRendering(true);

	//m_FogData
	m_FogData.darkness = 0.f;
	m_FogData.fadeDistance = 250.f;
	m_FogData.playerWorldPos = Vec3(0, 0, 0);
	m_FogData.sightRange = 250.f;
	m_FogData.smoothness = 0.f;
	m_FogData.time = 0.f;
}

void RenderManager::Render(vector<shared_ptr<GameObject>>& _gameObjects, bool _isShadowTech)
{
	if (m_useDeferredRendering && !_isShadowTech) {
		RenderDeferred(_gameObjects, _isShadowTech);
	}
	else {
		RenderForward(_gameObjects, _isShadowTech);
	}
}

//이 게임 오브젝트들 중에서 실질적으로 인스턴싱 되어야 하는 부분만 여기서. 
void RenderManager::RenderForward(vector<shared_ptr<GameObject>>& _gameObjects, bool _isShadowTech)
{
	ClearData();

	m_isShadowTech = _isShadowTech;

	RenderMeshRendererForward(_gameObjects);
	RenderModelRendererForward(_gameObjects);
	RenderAnimRendererForward(_gameObjects);

	//파티클 시스템 있는 것들 선별. 
	for (shared_ptr<GameObject>& gameObject : _gameObjects) {
		shared_ptr<ParticleSystem> particle = gameObject->GetFixedComponent<ParticleSystem>(ComponentType::ParticleSystem);

		if (particle != nullptr) {
			particle->Render(m_isShadowTech);
		}
		
		shared_ptr<Billboard> billboard = gameObject->GetFixedComponent<Billboard>(ComponentType::Billboard);
		if (billboard != nullptr)
			billboard->Render(m_isShadowTech);

		shared_ptr<SnowBillboard> snowBillboard = gameObject->GetFixedComponent<SnowBillboard>(ComponentType::SnowBillboard);
		if (snowBillboard != nullptr)
			snowBillboard->Render(m_isShadowTech);
	}
}

void RenderManager::RenderDeferred(vector<shared_ptr<GameObject>>& _gameObjects, bool _isShadowTech)
{
	ClearData();

	GRAPHICS->ClearDepthStencilView();
	GRAPHICS->ClearGBufferView();
	m_isShadowTech = _isShadowTech;

	// 1단계: G-Buffer 패스
	GRAPHICS->BeginGeometryPass();
	RenderGeometryPass(_gameObjects);

	//2단계: 디퍼드 라이팅 패스
	GRAPHICS->BeginLightingPass();
	RenderDeferredLighting();

	RenderOutlinePostProcess();

	// 3단계: 투명 객체 (포워드 방식)
	RenderTransparentObjects(_gameObjects);

	//RenderDecals(_gameObjects);

	// 4단계: 파티클 시스템 등
	for (shared_ptr<GameObject>& gameObject : _gameObjects) {
		shared_ptr<ParticleSystem> particle = gameObject->GetFixedComponent<ParticleSystem>(ComponentType::ParticleSystem);
		if (particle != nullptr) {
			particle->Render(false);
		}

		shared_ptr<Billboard> billboard = gameObject->GetFixedComponent<Billboard>(ComponentType::Billboard);
		if (billboard != nullptr)
			billboard->Render(false);

		shared_ptr<SnowBillboard> snowBillboard = gameObject->GetFixedComponent<SnowBillboard>(ComponentType::SnowBillboard);
		if (snowBillboard != nullptr)
			snowBillboard->Render(false);
	}
}

void RenderManager::ClearData()
{
	for (auto& pair : m_buffers) {
		pair.second->ClearData();
	}

}

void RenderManager::RenderMeshRendererForward(vector<shared_ptr<GameObject>>& _gameObjects)
{

	////인게임에 들어온 모든 아이들을 검사. 
	//map<InstanceID, vector<shared_ptr<GameObject>>> cache;


	////분류 단계
	//for (shared_ptr<GameObject>& gameObject : _gameObjects) {
	//	if (gameObject->GetMeshRenderer() == nullptr)
	//		continue;

	//	//그 매쉬에 대한 포인터 값을 기반으로 ID값을 가져옴
	//	//MESH와 MATERIAL 2개를 기반으로 ID값. 
	//	const InstanceID instanceID = gameObject->GetMeshRenderer()->GetInstanceID();
	//	cache[instanceID].push_back(gameObject);
	//}

	 // 순서를 보장하는 vector 사용
	vector<pair<InstanceID, vector<shared_ptr<GameObject>>>> cache;

	for (shared_ptr<GameObject>& gameObject : _gameObjects) {
		if (gameObject->GetMeshRenderer() == nullptr)
			continue;

		//auto material = gameObject->GetMeshRenderer()->GetMaterial();
		//if (material && material->IsDecalMaterial())
		//	continue;

		const InstanceID instanceID = gameObject->GetMeshRenderer()->GetInstanceID();

		// 기존 그룹 찾기
		auto it = std::find_if(cache.begin(), cache.end(),
			[instanceID](const auto& pair) { return pair.first == instanceID; });

		if (it != cache.end()) {
			it->second.push_back(gameObject);
		}
		else {
			cache.emplace_back(instanceID, vector<shared_ptr<GameObject>>{gameObject});
		}
	}

	//다 분류가 끝나면 같은 물체별로. 
	for (auto& pair : cache) {
		const vector<shared_ptr<GameObject>>& vec = pair.second;

		{
			const InstanceID instanceID = pair.first;

			
			for (int32 idx = 0; idx < vec.size(); ++idx) {
				const shared_ptr<GameObject>& gameObject = vec[idx];
				InstancingData data;
				data.m_world = gameObject->GetTransform()->GetWorldMatrix();

				AddData(instanceID, data);
			}

			//이제 그려주기. 
			shared_ptr<InstancingBuffer>& buffer = m_buffers[instanceID];

			//첫 번재 오브젝트한테, 얘가 그리도록 일 처리시키기. 
			vec[0]->GetMeshRenderer()->RenderInstancing(buffer, m_isShadowTech);
		}
	}
}

void RenderManager::RenderModelRendererForward(vector<shared_ptr<GameObject>>& _gameObjects)
{
	//인게임에 들어온 모든 아이들을 검사. 
	map<InstanceID, vector<shared_ptr<GameObject>>> cache;


	//분류 단계
	for (shared_ptr<GameObject>& gameObject : _gameObjects) {
		if (gameObject->GetModelRenderer() == nullptr)
			continue;

		//그 매쉬에 대한 포인터 값을 기반으로 ID값을 가져옴
		//MESH와 MATERIAL 2개를 기반으로 ID값. 
		const InstanceID instanceID = gameObject->GetModelRenderer()->GetInstanceID();
		cache[instanceID].push_back(gameObject);
	}

	//다 분류가 끝나면 같은 물체별로. 
	for (auto& pair : cache) {
		const vector<shared_ptr<GameObject>>& vec = pair.second;

		{
			const InstanceID instanceID = pair.first;


			for (int32 idx = 0; idx < vec.size(); ++idx) {
				const shared_ptr<GameObject>& gameObject = vec[idx];
				InstancingData data;
				data.m_world = gameObject->GetTransform()->GetWorldMatrix();

				AddData(instanceID, data);
			}

			//이제 그려주기. 
			shared_ptr<InstancingBuffer>& buffer = m_buffers[instanceID];

			//첫 번재 오브젝트한테, 얘가 그리도록 일 처리시키기. 
			vec[0]->GetModelRenderer()->RenderInstancing(buffer, m_isShadowTech);
		}
	}
}

void RenderManager::RenderAnimRendererForward(vector<shared_ptr<GameObject>>& _gameObjects)
{

	//인게임에 들어온 모든 아이들을 검사. 
	map<InstanceID, vector<shared_ptr<GameObject>>> cache;


	//분류 단계
	for (shared_ptr<GameObject>& gameObject : _gameObjects) {
		if (gameObject->GetModelAnimator() == nullptr)
			continue;

		//그 매쉬에 대한 포인터 값을 기반으로 ID값을 가져옴
		//MESH와 MATERIAL 2개를 기반으로 ID값. 
		const InstanceID instanceID = gameObject->GetModelAnimator()->GetInstanceID();
		cache[instanceID].push_back(gameObject);
	}

	//다 분류가 끝나면 같은 물체별로. 
	for (auto& pair : cache) {
		const vector<shared_ptr<GameObject>>& vec = pair.second;
		shared_ptr<InstancedTweenDesc> tweenDesc = make_shared<InstancedTweenDesc>();

		{
			const InstanceID instanceID = pair.first;


			for (int32 idx = 0; idx < vec.size(); ++idx) {
				const shared_ptr<GameObject>& gameObject = vec[idx];
				InstancingData data;
				data.m_world = gameObject->GetTransform()->GetWorldMatrix();

				AddData(instanceID, data);

				//INSTANCING TWEEN
				

				// Keep the existing animation sample in the shadow pass.
				if (!m_isShadowTech) {
					gameObject->GetModelAnimator()->UpdateTweenData();
				}


				tweenDesc->tweens[idx] = gameObject->GetModelAnimator()->GetTweenDesc();
				
			}

			//RENDER->PushTweenData(*tweenDesc.get());
			vec[0]->GetModelAnimator()->GetShader()->PushTweenData(*tweenDesc.get());

			//이제 그려주기. 
			shared_ptr<InstancingBuffer>& buffer = m_buffers[instanceID];

			//첫 번재 오브젝트한테, 얘가 그리도록 일 처리시키기. 
			vec[0]->GetModelAnimator()->RenderInstancing(buffer, m_isShadowTech);
		}
	}
}

void RenderManager::RenderMeshRendererDeferred(vector<shared_ptr<GameObject>>& _gameObjects)
{
	map<InstanceID, vector<shared_ptr<GameObject>>> cache;

	// 분류 단계 (투명 객체 제외)
	for (shared_ptr<GameObject>& gameObject : _gameObjects) {
		if (gameObject->GetMeshRenderer() == nullptr)
			continue;

		// 투명 객체는 제외 (나중에 포워드로 처리)
		if (auto material = gameObject->GetMeshRenderer()->GetMaterial()) {
			if (material->IsTransparent())
				continue;
			
		}
		 
		const InstanceID instanceID = gameObject->GetMeshRenderer()->GetInstanceID();
		cache[instanceID].push_back(gameObject);
	}

	// 인스턴싱 렌더링
	for (auto& pair : cache) {
		const vector<shared_ptr<GameObject>>& vec = pair.second;
		const InstanceID instanceID = pair.first;

		for (int32 idx = 0; idx < vec.size(); ++idx) {
			const shared_ptr<GameObject>& gameObject = vec[idx];
			InstancingData data;
			data.m_world = gameObject->GetTransform()->GetWorldMatrix();
			AddData(instanceID, data);
		}

		shared_ptr<InstancingBuffer>& buffer = m_buffers[instanceID];

		// 디퍼드 렌더링 호출 (G-Buffer에 데이터 쓰기)
		vec[0]->GetMeshRenderer()->RenderInstancingDeferred(buffer, m_isShadowTech);
	}
}

void RenderManager::RenderModelRendererDeferred(vector<shared_ptr<GameObject>>& _gameObjects)
{
	map<InstanceID, vector<shared_ptr<GameObject>>> cache;

	// 분류 단계 (투명 객체 제외)
	for (shared_ptr<GameObject>& gameObject : _gameObjects) {
		if (gameObject->GetModelRenderer() == nullptr)
			continue;

		// 투명 객체는 제외
		if (auto material = gameObject->GetModelRenderer()->GetMaterial()) {
			if (material->IsTransparent())
				continue;
		}

		const InstanceID instanceID = gameObject->GetModelRenderer()->GetInstanceID();
		cache[instanceID].push_back(gameObject);
	}

	// 인스턴싱 렌더링
	for (auto& pair : cache) {
		const vector<shared_ptr<GameObject>>& vec = pair.second;
		const InstanceID instanceID = pair.first;

		for (int32 idx = 0; idx < vec.size(); ++idx) {
			const shared_ptr<GameObject>& gameObject = vec[idx];
			InstancingData data;
			data.m_world = gameObject->GetTransform()->GetWorldMatrix();
			AddData(instanceID, data);
		}

		shared_ptr<InstancingBuffer>& buffer = m_buffers[instanceID];

		// 디퍼드 렌더링 호출
		vec[0]->GetModelRenderer()->RenderInstancingDeferred(buffer, m_isShadowTech);
	}
}

void RenderManager::RenderAnimRendererDeferred(vector<shared_ptr<GameObject>>& _gameObjects)
{
	map<InstanceID, vector<shared_ptr<GameObject>>> cache;

	// 분류 단계 (투명 객체 제외)
	for (shared_ptr<GameObject>& gameObject : _gameObjects) {
		if (gameObject->GetModelAnimator() == nullptr)
			continue;

		// 투명 객체는 제외
		auto modelAnimator = gameObject->GetModelAnimator();
		if (auto material = modelAnimator->GetMaterial()) {
			if (material->IsTransparent())
				continue;
		}

		const InstanceID instanceID = gameObject->GetModelAnimator()->GetInstanceID();
		cache[instanceID].push_back(gameObject);
	}

	// 인스턴싱 렌더링
	for (auto& pair : cache) {
		const vector<shared_ptr<GameObject>>& vec = pair.second;
		shared_ptr<InstancedTweenDesc> tweenDesc = make_shared<InstancedTweenDesc>();
		const InstanceID instanceID = pair.first;

		for (int32 idx = 0; idx < vec.size(); ++idx) {
			const shared_ptr<GameObject>& gameObject = vec[idx];
			InstancingData data;
			data.m_world = gameObject->GetTransform()->GetWorldMatrix();
			AddData(instanceID, data);

			// INSTANCING TWEEN 데이터 수집
			gameObject->GetModelAnimator()->UpdateTweenData();
			tweenDesc->tweens[idx] = gameObject->GetModelAnimator()->GetTweenDesc();
		}

		// G-Buffer 셰이더에 TweenDesc 전송
		if (vec[0]->GetModelAnimator()->GetShader()) {
			vec[0]->GetModelAnimator()->GetShader()->PushTweenData(*tweenDesc.get());
		}

		shared_ptr<InstancingBuffer>& buffer = m_buffers[instanceID];

		// 디퍼드 렌더링 호출
		vec[0]->GetModelAnimator()->RenderInstancingDeferred(buffer, m_isShadowTech);
	}
}

void RenderManager::AddData(InstanceID _instanceID, InstancingData& _data)
{
	if (m_buffers.find(_instanceID) == m_buffers.end()) {
		m_buffers[_instanceID] = make_shared<InstancingBuffer>();
	}

	m_buffers[_instanceID]->AddData(_data);
}

void RenderManager::RenderGeometryPass(vector<shared_ptr<GameObject>>& _gameObjects)
{

	RenderMeshRendererDeferred(_gameObjects);
	RenderModelRendererDeferred(_gameObjects);
	RenderAnimRendererDeferred(_gameObjects);
}

void RenderManager::RenderDeferredLighting()
{
	if (m_deferredLightingShader == nullptr)
		return;


	for (int i = 0; i < 4; ++i) {
		assert(GRAPHICS->m_gBufferSRVs[i] != nullptr);
	}
	
	GRAPHICS->BindGBufferSRVs(0);
	m_deferredLightingShader->SetTechnique(L"DeferredLightingTech");

	m_deferredLightingShader->GetSRV("ShadowMap")->SetResource(GRAPHICS->GetShadowMap()->GetComPtr().Get());
	m_deferredLightingShader->GetSRV("GBufferAlbedo")->SetResource(GRAPHICS->m_gBufferSRVs[0].Get());
	m_deferredLightingShader->GetSRV("GBufferNormal")->SetResource(GRAPHICS->m_gBufferSRVs[1].Get());
	m_deferredLightingShader->GetSRV("GBufferPosition")->SetResource(GRAPHICS->m_gBufferSRVs[2].Get());
	m_deferredLightingShader->GetSRV("GBufferMaterial")->SetResource(GRAPHICS->m_gBufferSRVs[3].Get());
	
	auto lightObj = CURSCENE->GetLight();

	m_deferredLightingShader->PushGlobalData(Camera::s_MatView, Camera::s_MatProjection);

	if (lightObj) {
		m_deferredLightingShader->PushLightData(lightObj->GetLight()->GetLightDesc());
	}

	m_deferredLightingShader->PushShadowData(Light::s_ShadowTransform);
	//  디버깅: 매트릭스 값 출력
	Matrix shadowMat = Light::s_ShadowTransform;

	m_deferredLightingShader->PushFOWData(m_FogData);

	DC->OMSetDepthStencilState(nullptr, 0);
	DC->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

	GRAPHICS->BindFullScreenQuad();
	m_deferredLightingShader->DrawIndexedInstancedCurTech(0, 6, 1, 0, 0, 0);


	ID3D11ShaderResourceView* nullSRVs[Graphics::GBUFFER_COUNT] = {nullptr};
	DC->PSSetShaderResources(0, Graphics::GBUFFER_COUNT, nullSRVs);
	m_deferredLightingShader->GetSRV("ShadowMap")->SetResource(nullptr);

	m_deferredLightingShader->GetSRV("GBufferAlbedo")->SetResource(nullptr);
	m_deferredLightingShader->GetSRV("GBufferNormal")->SetResource(nullptr);
	m_deferredLightingShader->GetSRV("GBufferPosition")->SetResource(nullptr);
	m_deferredLightingShader->GetSRV("GBufferMaterial")->SetResource(nullptr);
}

void RenderManager::RenderOutlinePostProcess()
{
	if (!m_outlineShader) {
		m_outlineShader = make_shared<Shader>(L"OutlinePostProcess");
	}
	// G-Buffer만 사용 (Scene Color 불필요)
	m_outlineShader->GetSRV("gNormalBuffer")->SetResource(GRAPHICS->m_gBufferSRVs[1].Get());
	m_outlineShader->GetSRV("gPositionBuffer")->SetResource(GRAPHICS->m_gBufferSRVs[2].Get());
	m_outlineShader->GetSRV("gMaterialBuffer")->SetResource(GRAPHICS->m_gBufferSRVs[3].Get());

	GRAPHICS->BindFullScreenQuad();
	m_outlineShader->DrawIndexedInstancedCurTech(0, 6, 1, 0, 0, 0);

	// 리소스 해제
	m_outlineShader->GetSRV("gNormalBuffer")->SetResource(nullptr);
	m_outlineShader->GetSRV("gPositionBuffer")->SetResource(nullptr);
	m_outlineShader->GetSRV("gMaterialBuffer")->SetResource(nullptr);
}

void RenderManager::RenderTransparentObjects(vector<shared_ptr<GameObject>>& _gameObjects)
{
	vector<shared_ptr<GameObject>> transparentObjects;

	// 투명 객체 선별 (기존 렌더러들을 통해 확인)
	for (auto& obj : _gameObjects) {
		bool isTransparent = false;

		// MeshRenderer 확인
		if (auto meshRenderer = obj->GetMeshRenderer()) {
			if (auto material = meshRenderer->GetMaterial()) {
				if (material->IsTransparent()) {
					isTransparent = true;
				}
			}
		}
		auto modelRenderer = obj->GetModelRenderer();
		// ModelRenderer 확인
		if (!isTransparent && modelRenderer) {
			if (auto material = modelRenderer->GetMaterial()) {
				if (material->IsTransparent()) {
					isTransparent = true;
				}
			}
		}
		auto modelAnimator = obj->GetModelAnimator();
		// ModelAnimator 확인
		if (!isTransparent && modelAnimator) {
			if (auto material = modelAnimator->GetMaterial()) {
				if (material->IsTransparent()) {
					isTransparent = true;
				}
			}
		}

		if (isTransparent) {
			transparentObjects.push_back(obj);
		}
	}

	if (transparentObjects.empty())
		return;

	// 깊이 정렬 (카메라로부터의 거리 기준)
	Vec3 cameraPos = Vec3(Camera::s_MatView._41, Camera::s_MatView._42, Camera::s_MatView._43);

	sort(transparentObjects.begin(), transparentObjects.end(),
		[cameraPos](const shared_ptr<GameObject>& a, const shared_ptr<GameObject>& b) {
		float distA = (a->GetTransform()->GetPosition() - cameraPos).Length();
		float distB = (b->GetTransform()->GetPosition() - cameraPos).Length();
		return distA > distB; // 먼 객체부터
	});

	// 포워드 렌더링으로 투명 객체들 렌더링
	RenderForward(transparentObjects, false);
}

void RenderManager::RenderDecals(vector<shared_ptr<GameObject>>& _gameObjects)
{
	map<InstanceID, vector<shared_ptr<GameObject>>> cache;

	// 데칼 객체 분류 (텍스처별로 자동 그룹핑됨)
	for (shared_ptr<GameObject>& gameObject : _gameObjects) {
		if (gameObject->GetMeshRenderer() == nullptr) continue;

		auto material = gameObject->GetMeshRenderer()->GetMaterial();
		if (!material || !material->IsDecalMaterial()) continue;

		// 텍스처가 다르면 다른 InstanceID가 됨
		const InstanceID instanceID = gameObject->GetMeshRenderer()->GetInstanceID();
		cache[instanceID].push_back(gameObject);
	}

	if (cache.empty())
		return;

	DC->OMSetDepthStencilState(GRAPHICS->GetDecalDepthStencilState().Get(), 0);
	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	DC->OMSetBlendState(GRAPHICS->GetDecalBlendState().Get(), blendFactor, 0xffffffff);

	for (auto& pair : cache) {
		const vector<shared_ptr<GameObject>>& vec = pair.second;
		const InstanceID instanceID = pair.first;

		for (int32 idx = 0; idx < vec.size(); ++idx) {
			const shared_ptr<GameObject>& gameObject = vec[idx];
			InstancingData data;
			data.m_world = gameObject->GetTransform()->GetWorldMatrix();
			AddData(instanceID, data);
		}

		shared_ptr<InstancingBuffer>& buffer = m_buffers[instanceID];
		vec[0]->GetMeshRenderer()->RenderInstancing(buffer, m_isShadowTech);
	}


	DC->OMSetDepthStencilState(nullptr, 0);
	DC->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}
